/*
 * Copyright (C) 2024-2025 Huawei Device Co., Ltd.
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

#include "trace_file_utils.h"

#include <cinttypes>
#include <cstdio>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <securec.h>
#include <unistd.h>

#include "common_utils.h"
#include "common_define.h"
#include "hilog/log.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D33
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "HitraceUtils"
#endif
namespace {
const int TIME_BUFFER_SIZE = 16;
const int DEFAULT_TRACE_DURATION = 30;
const int TIME_INIT = 1900;
static const char* TRACE_SNAPSHOT_PREFIX = "trace_";
static const char* TRACE_RECORDING_PREFIX = "record_trace_";
static const char* TRACE_CACHE_PREFIX = "cache_trace_";
std::map<TRACE_TYPE, std::string> tracePrefixMap = {
    {TRACE_TYPE::TRACE_SNAPSHOT, TRACE_SNAPSHOT_PREFIX},
    {TRACE_TYPE::TRACE_RECORDING, TRACE_RECORDING_PREFIX},
    {TRACE_TYPE::TRACE_CACHE, TRACE_CACHE_PREFIX},
};

uint64_t ConvertPageTraceTimeToUtTimeMs(const uint64_t& pageTraceTime)
{
    struct timespec bts = {0, 0};
    clock_gettime(CLOCK_BOOTTIME, &bts);
    uint64_t btNowMs = static_cast<uint64_t>(bts.tv_sec) * S_TO_MS + static_cast<uint64_t>(bts.tv_nsec) / MS_TO_NS;
    uint64_t utNowMs = static_cast<uint64_t>(std::time(nullptr)) * S_TO_MS;
    return utNowMs - btNowMs + pageTraceTime / MS_TO_NS;
}

std::string RegenerateTraceFileName(const std::string& fileName, const uint64_t& firstPageTraceTime,
    const uint64_t& traceDuration)
{
    std::string namePrefix;
    auto index = fileName.find(TRACE_SNAPSHOT_PREFIX);
    if (index == std::string::npos) {
        return "";
    }
    namePrefix = fileName.substr(0, index);
    uint64_t utFirstPageTraceTimeMs = ConvertPageTraceTimeToUtTimeMs(firstPageTraceTime);
    struct tm timeInfo = {};
    char timeBuf[TIME_BUFFER_SIZE] = {0};
    time_t utFirstPageTraceTimeSec = static_cast<time_t>(utFirstPageTraceTimeMs / S_TO_MS);
    if (localtime_r(&utFirstPageTraceTimeSec, &timeInfo) == nullptr) {
        HILOG_ERROR(LOG_CORE, "RegenerateTraceFileName: get local time failed");
        return "";
    }
    (void)strftime(timeBuf, TIME_BUFFER_SIZE, "%Y%m%d%H%M%S", &timeInfo);
    std::string newName = namePrefix + TRACE_SNAPSHOT_PREFIX + std::string(timeBuf) + "@" +
        std::to_string(firstPageTraceTime / S_TO_NS) + "-" + std::to_string(traceDuration) + ".sys";
    return newName;
}

bool GetStartAndEndTraceUtTimeFromFileName(const std::string& fileName, uint64_t& traceStartTime,
    uint64_t& traceEndTime)
{
    struct tm timeInfo = {};
    uint32_t number = 0;
    auto index = fileName.find(TRACE_SNAPSHOT_PREFIX);
    if (index == std::string::npos) {
        return false;
    }
    if (sscanf_s(fileName.substr(index, fileName.size() - index).c_str(), "trace_%4d%2d%2d%2d%2d%2d@%*[^-]-%u.sys",
                 &timeInfo.tm_year, &timeInfo.tm_mon, &timeInfo.tm_mday,
                 &timeInfo.tm_hour, &timeInfo.tm_min, &timeInfo.tm_sec,
                 &number) != 7) { // 7 : check sscanf_s return value
        HILOG_ERROR(LOG_CORE, "sscanf_s for trace file name failed.");
        return false;
    }
    timeInfo.tm_year -= TIME_INIT;
    timeInfo.tm_mon -= 1;
    time_t timestamp = mktime(&timeInfo);
    if (timestamp == -1) {
        HILOG_ERROR(LOG_CORE, "mktime failed to generate trace name timestamp.");
        return false;
    }
    traceStartTime = static_cast<uint64_t>(timestamp) * S_TO_MS;
    traceEndTime = traceStartTime + static_cast<uint64_t>(number);
    return true;
}

bool RenameTraceFile(const std::string& fileName, std::string& newFileName,
    const uint64_t& firstPageTraceTime, const uint64_t& lastPageTraceTime)
{
    if (firstPageTraceTime >= lastPageTraceTime) {
        HILOG_ERROR(LOG_CORE,
            "RenameTraceFile: firstPageTraceTime is larger than lastPageTraceTime, firstPageTraceTime:(%{public}"
            PRIu64 "), lastPageTraceTime:(%{public}" PRIu64 ")",
            firstPageTraceTime, lastPageTraceTime);
        return false;
    }
    HILOG_INFO(LOG_CORE, "RenameTraceFile: firstPageTraceTime:(%{public}" PRIu64
        "), lastPageTraceTime:(%{public}" PRIu64 ")", firstPageTraceTime, lastPageTraceTime);
    uint64_t traceDuration = (lastPageTraceTime - firstPageTraceTime) / MS_TO_NS;
    newFileName = RegenerateTraceFileName(fileName, firstPageTraceTime, traceDuration).c_str();
    if (newFileName == "") {
        HILOG_ERROR(LOG_CORE, "RenameTraceFile: RegenerateTraceFileName failed");
        return false;
    }
    if (rename(fileName.c_str(), newFileName.c_str())) {
        HILOG_ERROR(LOG_CORE, "RenameTraceFile: rename trace file failed, source file:%{public}s, new file:%{public}s.",
            fileName.c_str(), newFileName.c_str());
        return false;
    }
    HILOG_INFO(LOG_CORE, "RenameTraceFile: rename trace file success, source file:%{public}s, new file:%{public}s.",
        fileName.c_str(), newFileName.c_str());
    return true;
}
}

TraceFileInfo::TraceFileInfo(const std::string& name)
{
    filename = name;
}

TraceFileInfo::TraceFileInfo(const std::string& name, time_t time, int64_t sizekB, bool newFile)
{
    filename = name;
    ctime = time;
    fileSize = sizekB;
    isNewFile = newFile;
}

void GetTraceFilesInDir(std::vector<TraceFileInfo>& fileList, TRACE_TYPE traceType)
{
    if (!std::filesystem::exists(TRACE_FILE_DEFAULT_DIR) || !std::filesystem::is_directory(TRACE_FILE_DEFAULT_DIR)) {
        HILOG_INFO(LOG_CORE, "GetTraceFilesInDir fail, directory not exist");
        return;
    }
    struct stat fileStat;
    for (const auto &entry : std::filesystem::directory_iterator(TRACE_FILE_DEFAULT_DIR)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string fileName = entry.path().filename().string();
        if (fileName.substr(0, tracePrefixMap[traceType].size()) == tracePrefixMap[traceType]) {
            fileName = TRACE_FILE_DEFAULT_DIR + fileName;
            if (stat(fileName.c_str(), &fileStat) == 0) {
                fileList.emplace_back(fileName, fileStat.st_ctime, static_cast<uint64_t>(fileStat.st_size), false);
            }
        }
    }
    HILOG_INFO(LOG_CORE, "GetTraceFilesInDir fileList size: %{public}d.", static_cast<int>(fileList.size()));
    std::sort(fileList.begin(), fileList.end(), [](const TraceFileInfo& a, const TraceFileInfo& b) {
        return a.ctime < b.ctime;
    });
}

void GetTraceFileNamesInDir(std::set<std::string>& fileSet, TRACE_TYPE traceType)
{
    if (!std::filesystem::exists(TRACE_FILE_DEFAULT_DIR) || !std::filesystem::is_directory(TRACE_FILE_DEFAULT_DIR)) {
        HILOG_INFO(LOG_CORE, "GetTraceFileNamesInDir fail, directory not exist");
        return;
    }
    for (const auto &entry : std::filesystem::directory_iterator(TRACE_FILE_DEFAULT_DIR)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string fileName = entry.path().filename().string();
        if (fileName.substr(0, tracePrefixMap[traceType].size()) == tracePrefixMap[traceType]) {
            fileName = TRACE_FILE_DEFAULT_DIR + fileName;
            fileSet.emplace(fileName);
        }
    }
    HILOG_INFO(LOG_CORE, "GetTraceFileNamesInDir fileSet size: %{public}d.", static_cast<int>(fileSet.size()));
}

bool RemoveFile(const std::string& fileName)
{
    bool result = false;
    int fd = open(fileName.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        HILOG_WARN(LOG_CORE, "RemoveFile :: open file failed: %{public}s", fileName.c_str());
        return result;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
        HILOG_WARN(LOG_CORE, "RemoveFile :: get file lock failed, skip remove: %{public}s", fileName.c_str());
        close(fd);
        return result;
    }
    if (remove(fileName.c_str()) == 0) {
        HILOG_INFO(LOG_CORE, "RemoveFile :: Delete %{public}s success.", fileName.c_str());
        result = true;
    } else {
        HILOG_WARN(LOG_CORE, "RemoveFile :: Delete %{public}s failed.", fileName.c_str());
    }
    flock(fd, LOCK_UN);
    close(fd);
    return result;
}

std::string GenerateTraceFileName(TRACE_TYPE traceType)
{
    // eg: /data/log/hitrace/trace_localtime@boottime.sys
    std::string name = TRACE_FILE_DEFAULT_DIR;
    name += tracePrefixMap[traceType];
    // get localtime
    time_t currentTime = time(nullptr);
    struct tm timeInfo = {};
    char timeBuf[TIME_BUFFER_SIZE] = {0};
    if (localtime_r(&currentTime, &timeInfo) == nullptr) {
        HILOG_INFO(LOG_CORE, "GenerateTraceFileName: get loacl time failed");
        return "";
    }
    (void)strftime(timeBuf, TIME_BUFFER_SIZE, "%Y%m%d%H%M%S", &timeInfo);
    name += std::string(timeBuf);
    // get boottime
    struct timespec bts = {0, 0};
    clock_gettime(CLOCK_BOOTTIME, &bts);
    name += "@" + std::to_string(bts.tv_sec) + "-" + std::to_string(bts.tv_nsec) + ".sys";

    struct timespec mts = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &mts);
    HILOG_INFO(LOG_CORE, "output trace: %{public}s, boot_time(%{public}" PRId64 "), mono_time(%{public}" PRId64 ").",
        name.c_str(), static_cast<int64_t>(bts.tv_sec), static_cast<int64_t>(mts.tv_sec));
    return name;
}

std::string GenerateTraceFileNameByTraceTime(TRACE_TYPE traceType,
    const uint64_t& firstPageTraceTime, const uint64_t& lastPageTraceTime)
{
    if (firstPageTraceTime >= lastPageTraceTime) {
        HILOG_ERROR(LOG_CORE,
            "RenameTraceFile: firstPageTraceTime is larger than lastPageTraceTime, firstPageTraceTime:(%{public}"
            PRIu64 "), lastPageTraceTime:(%{public}" PRIu64 ")",
            firstPageTraceTime, lastPageTraceTime);
        return ""; // todo
    }
    // eg: /data/log/hitrace/trace_localtime@boottime.sys
    std::string name = TRACE_FILE_DEFAULT_DIR;
    name += tracePrefixMap[traceType];
    // get localtime
    uint64_t traceDuration = (lastPageTraceTime - firstPageTraceTime) / MS_TO_NS;
    uint64_t utFirstPageTraceTimeMs = ConvertPageTraceTimeToUtTimeMs(firstPageTraceTime);
    time_t utFirstPageTraceTimeSec = static_cast<time_t>(utFirstPageTraceTimeMs / S_TO_MS);
    struct tm timeInfo = {};
    char timeBuf[TIME_BUFFER_SIZE] = {0};
    if (localtime_r(&utFirstPageTraceTimeSec, &timeInfo) == nullptr) {
        HILOG_ERROR(LOG_CORE, "RegenerateTraceFileName: get local time failed");
        return "";
    }
    (void)strftime(timeBuf, TIME_BUFFER_SIZE, "%Y%m%d%H%M%S", &timeInfo);
    name += std::string(timeBuf);
    name += "@" + std::to_string(firstPageTraceTime / S_TO_NS) + "-" + std::to_string(traceDuration) + ".sys";
    return name;
}

/**
 * When the raw trace is started, clear the saved_events_format files in the folder.
 */
void DelSavedEventsFormat()
{
    const std::string savedEventsFormatPath = TRACE_FILE_DEFAULT_DIR + TRACE_SAVED_EVENTS_FORMAT;
    if (access(savedEventsFormatPath.c_str(), F_OK) != 0) {
        // saved_events_format not exit
        return;
    }
    // saved_events_format exit
    if (remove(savedEventsFormatPath.c_str()) == 0) {
        HILOG_INFO(LOG_CORE, "Delete saved_events_format success.");
    } else {
        HILOG_ERROR(LOG_CORE, "Delete saved_events_format failed.");
    }
}

void ClearCacheTraceFileByDuration(std::vector<TraceFileInfo>& cacheFileVec)
{
    if (cacheFileVec.empty()) {
        HILOG_INFO(LOG_CORE, "ClearCacheTraceFileByDuration: no cache file need to be deleted.");
        return;
    }
    uint64_t currentTime = static_cast<uint64_t>(std::time(nullptr));
    int index = 0;
    while (index < static_cast<int>(cacheFileVec.size())) {
        auto it = cacheFileVec.at(index);
        if ((it.traceEndTime < currentTime - static_cast<uint64_t>(DEFAULT_TRACE_DURATION)) ||
            (it.traceEndTime > currentTime)) {
            if (remove(it.filename.c_str()) == 0) {
                HILOG_INFO(LOG_CORE, "ClearCacheTraceFileByDuration: delete first: %{public}s success.",
                    it.filename.c_str());
            } else {
                HILOG_ERROR(LOG_CORE,
                    "ClearCacheTraceFileByDuration: delete first: %{public}s failed, errno: %{public}d.",
                    it.filename.c_str(), errno);
            }
            cacheFileVec.erase(cacheFileVec.begin() + index);
        } else {
            ++index;
        }
    }
}

void ClearCacheTraceFileBySize(std::vector<TraceFileInfo>& cacheFileVec, const uint64_t& fileSizeLimit)
{
    if (cacheFileVec.empty()) {
        HILOG_INFO(LOG_CORE, "ClearCacheTraceFileBySize: no cache file need to be deleted.");
        return;
    }
    int64_t totalCacheFileSize = 0;
    for (size_t i = 0; i < cacheFileVec.size(); ++i) {
        totalCacheFileSize += cacheFileVec[i].fileSize;
    }
    while (totalCacheFileSize > static_cast<int64_t>(fileSizeLimit)) {
        if (cacheFileVec.empty()) {
            HILOG_INFO(LOG_CORE, "ClearCacheTraceFileBySize: cacheFileVec is empty.");
            return;
        }
        auto it = cacheFileVec.begin();
        HILOG_INFO(LOG_CORE, "filename:%{public}s, fileSizeLimit:(%{public}" PRId64
            "), totalCacheFileSize:(%{public}" PRId64 "), fileSize:(%{public}" PRId64 ").",
            (*it).filename.c_str(), fileSizeLimit, totalCacheFileSize, (*it).fileSize);
        if (remove((*it).filename.c_str()) == 0) {
            totalCacheFileSize = totalCacheFileSize - (*it).fileSize;
            HILOG_INFO(LOG_CORE, "ClearCacheTraceFileBySize: delete first %{public}s success.", (*it).filename.c_str());
        } else {
            HILOG_ERROR(LOG_CORE, "ClearCacheTraceFileBySize: delete first: %{public}s failed, errno: %{public}d",
                (*it).filename.c_str(), errno);
        }
        cacheFileVec.erase(it);
    }
}

off_t GetFileSize(const std::string& filePath)
{
    struct stat fileInfo;
    int ret = stat(filePath.c_str(), &fileInfo);
    if (ret != 0) {
        HILOG_ERROR(LOG_CORE, "Get fileSize failed, %{public}s is not exist, ret is %{public}d.",
            filePath.c_str(), ret);
        return -1;
    }
    HILOG_INFO(LOG_CORE, "GetFileSize: filename:%{public}s, fileSize:(%{public}" PRId64 ").",
        filePath.c_str(), fileInfo.st_size);
    return fileInfo.st_size;
}

uint64_t GetCurUnixTimeMs()
{
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

void RefreshTraceVec(std::vector<TraceFileInfo>& traceVec, const TRACE_TYPE traceType)
{
    traceVec.clear();
    std::vector<TraceFileInfo> traceFileList;
    GetTraceFilesInDir(traceFileList, traceType);
    for (size_t i = 0; i < traceFileList.size(); i++) {
        TraceFileInfo traceFileInfo;
        traceFileInfo.filename = traceFileList[i].filename;
        if (!GetStartAndEndTraceUtTimeFromFileName(traceFileInfo.filename, traceFileInfo.traceStartTime,
            traceFileInfo.traceEndTime)) {
            continue;
        }
        traceFileInfo.fileSize = traceFileList[i].fileSize;
        traceVec.push_back(traceFileInfo);
        HILOG_INFO(LOG_CORE,
            "RefreshTraceVec::filename:%{public}s, traceStartTime:(%{public}" PRIu64
            "), traceEndTime:(%{public}" PRIu64 "), filesize:(%{public}" PRIu64 ").",
            traceFileInfo.filename.c_str(), traceFileInfo.traceStartTime,
            traceFileInfo.traceEndTime, traceFileInfo.fileSize);
    }
}

std::string RenameCacheFile(const std::string& cacheFile)
{
    std::string fileName = cacheFile.substr(cacheFile.find_last_of("/") + 1);
    std::string::size_type pos = fileName.find(CACHE_FILE_PREFIX);
    if (pos == std::string::npos) {
        return cacheFile;
    }
    std::string dirPath = cacheFile.substr(0, cacheFile.find_last_of("/") + 1);
    std::string newFileName = fileName.substr(pos + CACHE_FILE_PREFIX.size());
    std::string newFilePath = dirPath + newFileName;
    if (rename(cacheFile.c_str(), newFilePath.c_str()) != 0) {
        HILOG_ERROR(LOG_CORE, "rename %{public}s to %{public}s failed, errno: %{public}d.",
            cacheFile.c_str(), newFilePath.c_str(), errno);
        return cacheFile;
    }
    HILOG_INFO(LOG_CORE, "rename %{public}s to %{public}s success.", cacheFile.c_str(), newFilePath.c_str());
    return newFilePath;
}

bool SetFileInfo(const bool isFileExist, const std::string outPath, const uint64_t& firstPageTimestamp,
    const uint64_t& lastPageTimestamp, TraceFileInfo& traceFileInfo)
{
    std::string newFileName = outPath;
    if (isFileExist && !RenameTraceFile(outPath, newFileName, firstPageTimestamp, lastPageTimestamp)) {
        HILOG_INFO(LOG_CORE, "rename failed, outPath: %{public}s.", outPath.c_str());
        return false;
    }
    traceFileInfo.filename = newFileName;
    traceFileInfo.traceStartTime = ConvertPageTraceTimeToUtTimeMs(firstPageTimestamp);
    traceFileInfo.traceEndTime = ConvertPageTraceTimeToUtTimeMs(lastPageTimestamp);
    if (isFileExist) {
        traceFileInfo.fileSize = GetFileSize(newFileName);
    } else {
        traceFileInfo.fileSize = 0;
    }
    return true;
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS
