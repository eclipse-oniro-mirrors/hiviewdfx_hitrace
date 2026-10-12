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
}

class TraceSourcePolicyTest : public testing::Test {
public:
    void SetUp() override
    {
        char directory[] = "/data/local/tmp/trace_source_policy_XXXXXX";
        ASSERT_NE(mkdtemp(directory), nullptr);
        directory_ = directory;
    }

    void TearDown() override
    {
        TraceDumpState::GetInstance().EndLoopDumpSelf();
        if (directory_.empty()) {
            return;
        }
        EXPECT_TRUE(TraverseFiles(directory_, false, [](const char* path, const dirent* entry) {
            const std::string file = std::string(path) + "/" + entry->d_name;
            EXPECT_EQ(remove(file.c_str()), 0);
        }));
        EXPECT_EQ(rmdir(directory_.c_str()), 0);
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
        EXPECT_EQ(FileAgeingUtils::AgeTraceFiles(files, RETENTION_SECONDS, directory_), 0);
        EXPECT_EQ(access(file.c_str(), F_OK), 0);
        factory.reset();
        EXPECT_EQ(FileAgeingUtils::AgeTraceFiles(files, RETENTION_SECONDS, directory_), 0);
        EXPECT_NE(access(file.c_str(), F_OK), 0);
        EXPECT_TRUE(files.empty());
    }

protected:
    std::string directory_;
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
    const auto promotedFile = RenameCacheFile(file);
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
}
