/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
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

#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/xattr.h>
#include <unistd.h>
#include <vector>

#include "file_ageing_utils.h"
#include "trace_dump_state.h"
#include "trace_dump_strategy.h"
#include "trace_file_utils.h"

using namespace testing::ext;

namespace OHOS::HiviewDFX::Hitrace {
namespace {
constexpr char PARTIAL_TRACE[] = "partial trace";
constexpr uint32_t RETENTION_SECONDS = 1;

class FailingAsyncContent : public ITraceCpuRawWrite {
public:
    FailingAsyncContent(int fd, const std::string& path, uint64_t taskId)
        : ITraceCpuRawWrite(fd, path, taskId, false) {}

    bool WriteTraceContent() override
    {
        EXPECT_EQ(write(traceFileFd_, PARTIAL_TRACE, sizeof(PARTIAL_TRACE)),
            static_cast<ssize_t>(sizeof(PARTIAL_TRACE)));
        return false;
    }
};

class FailingCacheContent : public ITraceCpuRawContent {
public:
    FailingCacheContent(int fd, const std::string& path, const TraceDumpRequest& request)
        : ITraceCpuRawContent(fd, path, false, request) {}

    bool WriteTraceContent() override
    {
        EXPECT_EQ(write(traceFileFd_, PARTIAL_TRACE, sizeof(PARTIAL_TRACE)),
            static_cast<ssize_t>(sizeof(PARTIAL_TRACE)));
        dumpStatus_ = TraceErrorCode::WRITE_TRACE_INFO_ERROR;
        return false;
    }
};

class ExitingAsyncContent : public FailingAsyncContent {
public:
    ExitingAsyncContent(int fd, const std::string& path, uint64_t taskId)
        : FailingAsyncContent(fd, path, taskId) {}

    bool WriteTraceContent() override
    {
        FailingAsyncContent::WriteTraceContent();
        _exit(0);
    }
};

class ExitingSourceFactory : public TraceSourceLinuxFactory {
public:
    explicit ExitingSourceFactory(const std::string& path) : TraceSourceLinuxFactory(path) {}

    std::unique_ptr<ITraceCpuRawWrite> GetTraceCpuRawWrite(uint64_t taskId) override
    {
        return std::make_unique<ExitingAsyncContent>(traceFileFd_.GetFd(), traceFilePath_, taskId);
    }
};

class FailingSourceFactory : public TraceSourceLinuxFactory {
public:
    explicit FailingSourceFactory(const std::string& path, bool failInitialization = false)
        : TraceSourceLinuxFactory(path), failInitialization_(failInitialization) {}

    std::unique_ptr<ITraceFileHdrContent> GetTraceFileHeader() override
    {
        if (failInitialization_) {
            return nullptr;
        }
        return TraceSourceLinuxFactory::GetTraceFileHeader();
    }

    std::unique_ptr<ITraceCpuRawWrite> GetTraceCpuRawWrite(uint64_t taskId) override
    {
        return std::make_unique<FailingAsyncContent>(traceFileFd_.GetFd(), traceFilePath_, taskId);
    }

    std::unique_ptr<ITraceCpuRawContent> GetTraceCpuRaw(const TraceDumpRequest& request) override
    {
        return std::make_unique<FailingCacheContent>(traceFileFd_.GetFd(), traceFilePath_, request);
    }

private:
    bool failInitialization_;
};

class AsyncFailureStrategy : public AsyncTraceWriteStrategy {
protected:
    bool NeedDoPreAndPost() const override { return false; }
    bool NeedCreateTraceContentPtr() const override { return false; }
};

class CacheFailureStrategy : public CacheTraceDumpStrategy {
protected:
    bool NeedDoPreAndPost() const override { return false; }
};

class RecordFailureStrategy : public RecordTraceDumpStrategy {
protected:
    bool NeedDoPreAndPost() const override { return false; }
};

void ExpectSource(const std::string& file, const std::string& expected)
{
    char source[16] = {};
    ASSERT_EQ(getxattr(file.c_str(), "user.trace_source", source, sizeof(source)),
        static_cast<ssize_t>(expected.size()));
    EXPECT_STREQ(source, expected.c_str());
}

void ExpectNoSource(const std::string& file)
{
    char source[16] = {};
    const ssize_t result = getxattr(file.c_str(), "user.trace_source", source, sizeof(source));
    const int savedErrno = errno;
    EXPECT_EQ(result, -1);
    EXPECT_EQ(savedErrno, ENODATA);
}
}

class TraceSourcePolicyTest : public testing::Test {
public:
    void SetUp() override
    {
        char directory[] = "/data/log/hitrace/source_policy_XXXXXX";
        ASSERT_NE(mkdtemp(directory), nullptr);
        directory_ = directory;
        char externalDirectory[] = "/data/local/tmp/trace_source_policy_XXXXXX";
        ASSERT_NE(mkdtemp(externalDirectory), nullptr);
        externalDirectory_ = externalDirectory;
    }

    void TearDown() override
    {
        TraceDumpState::GetInstance().EndLoopDumpSelf();
        for (const auto& directory : {directory_, externalDirectory_}) {
            if (directory.empty()) {
                continue;
            }
            EXPECT_TRUE(TraverseFiles(directory, false, [](const char* path, const dirent* entry) {
                const std::string file = std::string(path) + "/" + entry->d_name;
                EXPECT_EQ(remove(file.c_str()), 0);
            }));
            EXPECT_EQ(rmdir(directory.c_str()), 0);
        }
    }

    void CheckSystemFileAgeing(std::shared_ptr<FailingSourceFactory>& factory, const std::string& file)
    {
        constexpr char expectedSource[] = "system";
        char source[sizeof(expectedSource)] = {};
        const ssize_t length = getxattr(file.c_str(), "user.trace_source", source, sizeof(source));
        EXPECT_EQ(length, static_cast<ssize_t>(sizeof(expectedSource) - 1));
        EXPECT_STREQ(source, expectedSource);
        timespec times[2] = {};
        ASSERT_EQ(clock_gettime(CLOCK_REALTIME, &times[0]), 0);
        times[0].tv_sec -= RETENTION_SECONDS + 1;
        times[1] = times[0];
        ASSERT_EQ(utimensat(AT_FDCWD, file.c_str(), times, 0), 0);
        std::vector<TraceFileInfo> files {TraceFileInfo(file)};
        factory.reset();
        EXPECT_EQ(FileAgeingUtils::AgeTraceFiles(files, RETENTION_SECONDS, directory_), 0);
        EXPECT_NE(access(file.c_str(), F_OK), 0);
        EXPECT_TRUE(files.empty());
    }

protected:
    std::string directory_;
    std::string externalDirectory_;
};

HWTEST_F(TraceSourcePolicyTest, AsyncWriteFailureRetainsSystemSource, TestSize.Level1)
{
    const std::string file = directory_ + "/trace_failed.sys";
    auto factory = std::make_shared<FailingSourceFactory>(file);
    const TraceDumpRequest request {
        .type = TraceDumpType::TRACE_ASYNC_WRITE,
        .source = TraceFileSource::SYSTEM
    };
    const auto ret = AsyncFailureStrategy().Execute(factory, request);
    EXPECT_EQ(ret.code, TraceErrorCode::WRITE_TRACE_INFO_ERROR);
    EXPECT_GT(GetFileSize(file), 0);
    CheckSystemFileAgeing(factory, file);
}

HWTEST_F(TraceSourcePolicyTest, InitializationFailureRetainsSystemSource, TestSize.Level1)
{
    const std::string file = directory_ + "/trace_empty.sys";
    auto factory = std::make_shared<FailingSourceFactory>(file, true);
    const TraceDumpRequest request {.source = TraceFileSource::SYSTEM};
    const auto ret = SnapshotTraceDumpStrategy().Execute(factory, request);
    EXPECT_EQ(ret.code, TraceErrorCode::WRITE_TRACE_INFO_ERROR);
    EXPECT_EQ(GetFileSize(file), 0);
    CheckSystemFileAgeing(factory, file);
}

HWTEST_F(TraceSourcePolicyTest, CacheFailurePromotionRetainsSystemSource, TestSize.Level1)
{
    const std::string file = directory_ + "/cache_trace_failed.sys";
    auto factory = std::make_shared<FailingSourceFactory>(file);
    const TraceDumpRequest request {
        .type = TraceDumpType::TRACE_CACHE,
        .source = TraceFileSource::SYSTEM
    };
    ASSERT_TRUE(TraceDumpState::GetInstance().StartLoopDump());
    CacheFailureStrategy().Execute(factory, request);
    TraceDumpState::GetInstance().EndLoopDumpSelf();
    EXPECT_GT(GetFileSize(file), 0);
    ExpectSource(file, "system");
    factory.reset();
    const auto promotedFile = RenameCacheFile(file, TraceFileSource::SYSTEM);
    ASSERT_FALSE(promotedFile.empty());
    ASSERT_NE(promotedFile, file);
    CheckSystemFileAgeing(factory, promotedFile);
}

HWTEST_F(TraceSourcePolicyTest, ReplacementFileRetainsSystemSource, TestSize.Level1)
{
    const std::string file = directory_ + "/trace_replaced.sys";
    auto factory = std::make_shared<FailingSourceFactory>(directory_ + "/trace_initial.sys");
    const TraceDumpRequest request {
        .type = TraceDumpType::TRACE_ASYNC_WRITE,
        .source = TraceFileSource::SYSTEM
    };
    EXPECT_EQ(AsyncFailureStrategy().Execute(factory, request).code, TraceErrorCode::WRITE_TRACE_INFO_ERROR);
    ASSERT_TRUE(factory->UpdateTraceFile(file));
    EXPECT_EQ(AsyncFailureStrategy().Execute(factory, request).code, TraceErrorCode::WRITE_TRACE_INFO_ERROR);
    CheckSystemFileAgeing(factory, file);
}

HWTEST_F(TraceSourcePolicyTest, WriterExitRetainsSystemSource, TestSize.Level1)
{
    const std::string file = directory_ + "/trace_interrupted.sys";
    const pid_t pid = fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        auto factory = std::make_shared<ExitingSourceFactory>(file);
        const TraceDumpRequest request {
            .type = TraceDumpType::TRACE_ASYNC_WRITE,
            .source = TraceFileSource::SYSTEM
        };
        AsyncFailureStrategy().Execute(factory, request);
        _exit(EXIT_FAILURE);
    }
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(WEXITSTATUS(status), 0);
    EXPECT_GT(GetFileSize(file), 0);
    constexpr char expectedSource[] = "system";
    char source[sizeof(expectedSource)] = {};
    EXPECT_EQ(getxattr(file.c_str(), "user.trace_source", source, sizeof(source)),
        static_cast<ssize_t>(sizeof(expectedSource) - 1));
    EXPECT_STREQ(source, expectedSource);
    timespec times[2] = {};
    ASSERT_EQ(clock_gettime(CLOCK_REALTIME, &times[0]), 0);
    times[0].tv_sec -= RETENTION_SECONDS + 1;
    times[1] = times[0];
    ASSERT_EQ(utimensat(AT_FDCWD, file.c_str(), times, 0), 0);
    std::vector<TraceFileInfo> files {TraceFileInfo(file)};
    EXPECT_EQ(FileAgeingUtils::AgeTraceFiles(files, RETENTION_SECONDS, directory_), 0);
    EXPECT_NE(access(file.c_str(), F_OK), 0);
    EXPECT_TRUE(files.empty());
}

HWTEST_F(TraceSourcePolicyTest, FactoriesTagCreatedAndReplacementFiles, TestSize.Level1)
{
    int index = 0;
    for (const auto source : {TraceFileSource::SYSTEM, TraceFileSource::USER}) {
        for (const bool hm : {false, true}) {
            const std::string file = directory_ + "/trace_created_" + std::to_string(index++);
            std::shared_ptr<ITraceSourceFactory> factory;
            if (hm) {
                factory = std::make_shared<TraceSourceHMFactory>(file, source);
            } else {
                factory = std::make_shared<TraceSourceLinuxFactory>(file, source);
            }
            const std::string expected = source == TraceFileSource::SYSTEM ? "system" : "user";
            ExpectSource(file, expected);
            const std::string replacement = file + "_replacement";
            ASSERT_TRUE(factory->UpdateTraceFile(replacement));
            ExpectSource(replacement, expected);
        }
    }
}

HWTEST_F(TraceSourcePolicyTest, ReplacementUsesActualOutputDirectory, TestSize.Level1)
{
    int index = 0;
    for (const auto source : {TraceFileSource::SYSTEM, TraceFileSource::USER}) {
        for (const bool hm : {false, true}) {
            const std::string name = "/trace_" + std::to_string(index++);
            const std::string externalFile = externalDirectory_ + name;
            std::shared_ptr<ITraceSourceFactory> factory;
            if (hm) {
                factory = std::make_shared<TraceSourceHMFactory>(externalFile, source);
            } else {
                factory = std::make_shared<TraceSourceLinuxFactory>(externalFile, source);
            }
            ASSERT_EQ(access(externalFile.c_str(), F_OK), 0);
            ExpectNoSource(externalFile);
            ASSERT_TRUE(factory->UpdateTraceFile(directory_ + name));
            ExpectSource(directory_ + name, source == TraceFileSource::SYSTEM ? "system" : "user");
            ASSERT_TRUE(factory->UpdateTraceFile(externalFile));
            ExpectNoSource(externalFile);
        }
    }
}

HWTEST_F(TraceSourcePolicyTest, UserInitializationFailureRemainsExcludedFromSystemAgeing, TestSize.Level1)
{
    const std::string file = directory_ + "/trace_user_failed.sys";
    auto factory = std::make_shared<FailingSourceFactory>(file, true);
    const TraceDumpRequest request {.source = TraceFileSource::USER};
    EXPECT_EQ(SnapshotTraceDumpStrategy().Execute(factory, request).code, TraceErrorCode::WRITE_TRACE_INFO_ERROR);
    EXPECT_EQ(GetFileSize(file), 0);
    ExpectSource(file, "user");
    factory.reset();
    timespec times[2] = {};
    ASSERT_EQ(utimensat(AT_FDCWD, file.c_str(), times, 0), 0);
    std::vector<TraceFileInfo> files {TraceFileInfo(file)};
    EXPECT_EQ(FileAgeingUtils::AgeTraceFiles(files, RETENTION_SECONDS, directory_), SUCCESS);
    EXPECT_EQ(access(file.c_str(), F_OK), 0);
    EXPECT_EQ(files.size(), 1u);
}

HWTEST_F(TraceSourcePolicyTest, RecordingWriteFailureRetainsUserSource, TestSize.Level1)
{
    const std::string file = directory_ + "/record_trace_failed.sys";
    auto factory = std::make_shared<FailingSourceFactory>(file);
    const TraceDumpRequest request {
        .type = TraceDumpType::TRACE_RECORDING,
        .source = TraceFileSource::USER
    };
    ASSERT_TRUE(TraceDumpState::GetInstance().StartLoopDump());
    EXPECT_EQ(RecordFailureStrategy().Execute(factory, request).code, TraceErrorCode::WRITE_TRACE_INFO_ERROR);
    TraceDumpState::GetInstance().EndLoopDumpSelf();
    EXPECT_GT(GetFileSize(file), 0);
    ExpectSource(file, "user");
}


HWTEST_F(TraceSourcePolicyTest, FailedOpenDoesNotRemoveExistingDirectory, TestSize.Level1)
{
    const std::string output = directory_ + "/existing";
    ASSERT_EQ(mkdir(output.c_str(), 0700), 0);
    for (const auto source : {TraceFileSource::SYSTEM, TraceFileSource::USER}) {
        auto factory = std::make_shared<TraceSourceLinuxFactory>(output, source);
        const TraceDumpRequest request {.source = source};
        EXPECT_EQ(SnapshotTraceDumpStrategy().Execute(factory, request).code, TraceErrorCode::WRITE_TRACE_INFO_ERROR);
        struct stat info {};
        ASSERT_EQ(stat(output.c_str(), &info), 0);
        EXPECT_TRUE(S_ISDIR(info.st_mode));
    }
    EXPECT_EQ(rmdir(output.c_str()), 0);
}

}
