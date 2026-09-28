/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "file_ageing_utils.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <memory>
#include <queue>
#include <set>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>

#include "hilog/log.h"

#include "common_utils.h"
#include "smart_fd.h"
#include "trace_file_utils.h"
#include "trace_json_parser.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceUtils"
#endif

namespace {
constexpr int BYTE_PER_KB = 1024;
constexpr size_t DEFAULT_LINK_NUM = 16;
using namespace OHOS::HiviewDFX::Hitrace;

bool IsTraceFileExpired(const timespec& modified, const timespec& now, uint32_t retentionSecond)
{
    const int64_t cutoff = static_cast<int64_t>(now.tv_sec) - static_cast<int64_t>(retentionSecond);
    return static_cast<int64_t>(modified.tv_sec) < cutoff ||
        (static_cast<int64_t>(modified.tv_sec) == cutoff && modified.tv_nsec < now.tv_nsec);
}

int AgeTraceFileAt(int directoryFd, const char* name, const timespec& now, uint32_t retentionSecond, bool& removed)
{
    OHOS::HiviewDFX::SmartFd fd(openat(directoryFd, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    if (!fd) {
        return (errno == ENOENT || errno == ELOOP) ? SUCCESS : FILE_ERROR;
    }
    struct stat fileStat {};
    if (fstat(fd.GetFd(), &fileStat) != 0) {
        return FILE_ERROR;
    }
    if (!S_ISREG(fileStat.st_mode) || !IsTraceFileExpired(fileStat.st_mtim, now, retentionSecond)) {
        return SUCCESS;
    }
    constexpr char sourceTag[] = "system";
    char source[sizeof(sourceTag) - 1] = {};
    const ssize_t length = TEMP_FAILURE_RETRY(fgetxattr(fd.GetFd(), "user.trace_source", source, sizeof(source)));
    if (length < 0) {
        return (errno == ENODATA || errno == ERANGE) ? SUCCESS : FILE_ERROR;
    }
    if (length != static_cast<ssize_t>(sizeof(source)) || memcmp(source, sourceTag, sizeof(source)) != 0) {
        return SUCCESS;
    }
    // RecoverTmpTrace holds this lock until its asynchronous zip task finishes.
    if (flock(fd.GetFd(), LOCK_EX | LOCK_NB) != 0) {
        return (errno == EWOULDBLOCK || errno == EAGAIN) ? SUCCESS : FILE_ERROR;
    }
    if (unlinkat(directoryFd, name, 0) != 0) {
        return errno == ENOENT ? SUCCESS : FILE_ERROR;
    }
    removed = true;
    HILOG_INFO(LOG_CORE, "AgeTraceFile: deleted %{public}s.", name);
    return SUCCESS;
}

class FileAgeingChecker {
public:
    virtual bool ShouldAgeing(const TraceFileInfo& traceFileInfo) = 0;
    virtual ~FileAgeingChecker() = default;

    static std::shared_ptr<FileAgeingChecker> CreateFileChecker(const TraceDumpType traceType,
        const CheckType checkType);
};

class FileNumberChecker : public FileAgeingChecker {
public:
    explicit FileNumberChecker(int64_t count) : maxCount_(count) {};

    bool ShouldAgeing(const TraceFileInfo& traceFileInfo) override
    {
        if (currentCount_ >= maxCount_) {
            return true;
        }
        currentCount_++;
        return false;
    }
private:
    int64_t currentCount_ = 0;
    const int64_t maxCount_;
};

class FileSizeChecker : public FileAgeingChecker {
public:
    explicit FileSizeChecker(int64_t sizeKb) : maxSizeKb_(sizeKb) {};

    bool ShouldAgeing(const TraceFileInfo& traceFileInfo) override
    {
        if (currentFileNumber_ < minFileNumber_) {
            currentFileNumber_++;
            currentSizeKb_ += traceFileInfo.fileSize / BYTE_PER_KB;
            return false;
        }

        if (currentSizeKb_ >= maxSizeKb_) {
            return true;
        }
        currentSizeKb_ += traceFileInfo.fileSize / BYTE_PER_KB;
        return false;
    }

private:
    int64_t currentSizeKb_ = 0;
    const int64_t maxSizeKb_;

    int64_t currentFileNumber_ = 0;
    const int64_t minFileNumber_ = 2;  // To prevent all files from being cleared, set a minimum file number limit
};

std::shared_ptr<FileAgeingChecker> FileAgeingChecker::CreateFileChecker(const TraceDumpType traceType,
    const CheckType checkType)
{
    if (traceType != TraceDumpType::TRACE_RECORDING && traceType != TraceDumpType::TRACE_SNAPSHOT) {
        return nullptr;
    }

    const AgeingParam& param = TraceJsonParser::Instance().GetAgeingParam(traceType);
    if (IsRootVersion() && !param.rootEnable) {
        return nullptr;
    }

    if (param.fileSizeKbLimit > 0 && checkType == CheckType::FILESIZE) {
        return std::make_shared<FileSizeChecker>(param.fileSizeKbLimit);
    }

    if (param.fileNumberLimit > 0 && checkType == CheckType::FILENUMBER) {
        return std::make_shared<FileNumberChecker>(param.fileNumberLimit);
    }

    return nullptr;
}

void HandleFileNotInVec(std::vector<TraceFileInfo>& fileList, const TraceDumpType traceType, int32_t& deleteCount)
{
    // handle files that are not saved in vector
    std::set<std::string> traceFiles = {};
    GetTraceFileNamesInDir(traceFiles, traceType);
    for (const auto& traceFileInfo : fileList) {
        traceFiles.erase(traceFileInfo.filename);
    }
    for (const auto& filename : traceFiles) {
        if (RemoveFile(filename)) {
            deleteCount++;
        }
    }
    HILOG_INFO(LOG_CORE, "HandleAgeing: deleteCount:%{public}d type:%{public}d",
               deleteCount, static_cast<int32_t>(traceType));
}

void HandleFileNeedAgeing(const int64_t& needDelete, const CheckType checkType, std::vector<TraceFileInfo>& fileList,
    std::set<std::string>& needRemoveFiles, std::queue<TraceFileInfo>& linkFiles)
{
    int64_t currCount = 0;
    for (const auto& fileInfo : fileList) {
        if (currCount >= needDelete) {
            break;
        }
        ssize_t len = TEMP_FAILURE_RETRY(getxattr(fileInfo.filename.c_str(), ATTR_NAME_LINK, nullptr, 0));
        if (len == -1) {
            needRemoveFiles.insert(fileInfo.filename);
        } else {
            linkFiles.push(fileInfo);
            if (linkFiles.size() <= DEFAULT_LINK_NUM) {
                continue;
            }
            auto linkfileInfo = linkFiles.front();
            needRemoveFiles.insert(linkfileInfo.filename);
            linkFiles.pop();
            if (checkType == CheckType::FILESIZE) {
                currCount += linkfileInfo.fileSize;
                continue;
            }
        }

        if (checkType == CheckType::FILESIZE) {
            currCount += fileInfo.fileSize;
        } else if (checkType == CheckType::FILENUMBER) {
            currCount++;
        } else {
            break;
        }
    }
}

void HandleAgeingImpl(std::vector<TraceFileInfo>& fileList, const TraceDumpType traceType, FileAgeingChecker& helper)
{
    int32_t deleteCount = 0;
    // handle the files saved in vector
    std::vector<TraceFileInfo> result = {};
    for (auto it = fileList.rbegin(); it != fileList.rend(); it++) {
        if (helper.ShouldAgeing(*it)) {
            if (RemoveFile(it->filename)) {
                deleteCount++;
            }
        } else {
            result.emplace_back(*it);
        }
    }
    fileList.assign(result.rbegin(), result.rend());
    HandleFileNotInVec(fileList, traceType, deleteCount);
}

static void CalculateFilesize(const std::vector<TraceFileInfo>& fileList, int64_t& countSize)
{
    size_t currLinkNum = 0;
    for (const auto &fileInfo : fileList) {
        if (TEMP_FAILURE_RETRY(getxattr(fileInfo.filename.c_str(), ATTR_NAME_LINK, nullptr, 0)) == -1) {
            countSize += fileInfo.fileSize;
        } else {
            if ((++currLinkNum) > DEFAULT_LINK_NUM) {
                countSize += fileInfo.fileSize;
            }
        }
    }
}

void HandleAgeingSnapShort(std::vector<TraceFileInfo>& fileList, const TraceDumpType traceType,
    const CheckType checkType)
{
    const AgeingParam& param = TraceJsonParser::Instance().GetAgeingParam(traceType);
    int64_t needDelete = 0;
    int32_t deleteCount = 0;
    std::set<std::string> needRemoveFiles = {};
    std::queue<TraceFileInfo> linkFiles= {};
    if (checkType == CheckType::FILESIZE) {
        int64_t countSize = 0;
        CalculateFilesize(fileList, countSize);
        needDelete = countSize - param.fileSizeKbLimit * BYTE_PER_KB;
        if (needDelete <= 0 || needDelete == countSize) {
            HandleFileNotInVec(fileList, traceType, deleteCount);
            return;
        }
        HandleFileNeedAgeing(needDelete, CheckType::FILESIZE, fileList, needRemoveFiles, linkFiles);
    } else if (checkType == CheckType::FILENUMBER) {
        needDelete = static_cast<int64_t>(fileList.size()) - param.fileNumberLimit;
        if (needDelete <= 0 || needDelete == static_cast<int64_t>(fileList.size())) {
            HandleFileNotInVec(fileList, traceType, deleteCount);
            return;
        }
        HandleFileNeedAgeing(needDelete, CheckType::FILENUMBER, fileList, needRemoveFiles, linkFiles);
    }
    
    for (auto iter = needRemoveFiles.begin(); iter != needRemoveFiles.end();) {
        if (RemoveFile(*iter)) {
            deleteCount++;
            ++iter;
        } else {
            iter = needRemoveFiles.erase(iter);
        }
    }
    fileList.erase(std::remove_if(fileList.begin(), fileList.end(),
        [&needRemoveFiles] (const TraceFileInfo& fileInfo) {
            return needRemoveFiles.count(fileInfo.filename) != 0;
        }), fileList.end());
    HandleFileNotInVec(fileList, traceType, deleteCount);
}
}  // namespace

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

int FileAgeingUtils::AgeTraceFiles(std::vector<TraceFileInfo>& fileList, uint32_t retentionSecond,
    const std::string& directory)
{
    timespec now {};
    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        return FILE_ERROR;
    }
    std::unique_ptr<DIR, int (*)(DIR*)> dir(opendir(directory.c_str()), closedir);
    if (!dir) {
        return errno == ENOENT ? SUCCESS : FILE_ERROR;
    }
    const std::string prefix = directory + (directory.back() == '/' ? "" : "/");
    std::set<std::string> removedFiles;
    int result = SUCCESS;
    dirent* entry = nullptr;
    do {
        errno = 0;
        entry = readdir(dir.get());
        if (entry == nullptr) {
            if (errno != 0) {
                result = FILE_ERROR;
            }
            break;
        }
        if (entry->d_type != DT_REG && entry->d_type != DT_UNKNOWN) {
            continue;
        }
        bool removed = false;
        if (AgeTraceFileAt(dirfd(dir.get()), entry->d_name, now, retentionSecond, removed) != SUCCESS) {
            HILOG_ERROR(LOG_CORE, "AgeTraceFile: failed to age %{public}s, errno %{public}d.", entry->d_name, errno);
            result = FILE_ERROR;
        }
        if (removed) {
            removedFiles.insert(prefix + entry->d_name);
        }
    } while (entry != nullptr);
    auto isRemoved = [&removedFiles](const TraceFileInfo& info) {
        return removedFiles.count(info.filename) != 0;
    };
    fileList.erase(std::remove_if(fileList.begin(), fileList.end(), isRemoved), fileList.end());
    return result;
}

void FileAgeingUtils::HandleAgeing(std::vector<TraceFileInfo>& fileList, const TraceDumpType traceType,
                                   int64_t setTotalSize)
{
    std::shared_ptr<FileAgeingChecker> checkerFilenumber = FileAgeingChecker::CreateFileChecker(traceType,
        CheckType::FILENUMBER);
    if (checkerFilenumber != nullptr) {
        if (traceType == TraceDumpType::TRACE_RECORDING) {
            HandleAgeingImpl(fileList, traceType, *checkerFilenumber);
        } else if (traceType == TraceDumpType::TRACE_SNAPSHOT) {
            HandleAgeingSnapShort(fileList, traceType, CheckType::FILENUMBER);
        }
    }
    if (traceType == TraceDumpType::TRACE_RECORDING && setTotalSize != 0) {
        FileSizeChecker checkerSetTotalSize(setTotalSize);
        HandleAgeingImpl(fileList, traceType, checkerSetTotalSize);
        return;
    }
    std::shared_ptr<FileAgeingChecker> checkerFilesize = FileAgeingChecker::CreateFileChecker(traceType,
        CheckType::FILESIZE);
    if (checkerFilesize != nullptr) {
        if (traceType == TraceDumpType::TRACE_RECORDING) {
            HandleAgeingImpl(fileList, traceType, *checkerFilesize);
        } else if (traceType == TraceDumpType::TRACE_SNAPSHOT) {
            HandleAgeingSnapShort(fileList, traceType, CheckType::FILESIZE);
        }
    }
}

} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
