/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
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

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "common_utils.h"
#include "file_ageing_utils.h"
#include "hitrace_dump.h"
#define private public
#include "trace_dump_executor.h"
#undef private
#include "trace_dump_pipe.h"
#include "trace_dump_state.h"
#include "trace_dump_strategy.h"
#include "trace_file_utils.h"

using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
namespace {
static const char* const TEST_TRACE_TEMP_FILE = "/data/local/tmp/test_trace_file";
constexpr int BYTE_PER_MB = 1024 * 1024;
constexpr int TIMEOUT_5S = 5;
constexpr uint64_t SYNC_RETURN_TIMEOUT_NS = 5000000000; // 5s
}

class TraceDumpExecutorTest : public testing::Test {
public:
    static void SetUpTestCase(void)
    {
        system("service_control stop hiview");
    }

    static void TearDownTestCase(void)
    {
        system("service_control start hiview");
    }

    void SetUp() {}
    void TearDown() {}
};

namespace {
static off_t GetFileSize(const std::string& file)
{
    struct stat fileStat;
    if (stat(file.c_str(), &fileStat) != 0) {
        GTEST_LOG_(ERROR) << "Failed to get file size of " << file;
        return 0;
    }
    return fileStat.st_size;
}

/**
 * @tc.name: TraceDumpExecutorTest001
 * @tc.desc: Test TraceDumpExecutor class StartDumpTraceLoop/StopDumpTraceLoop function.
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpExecutorTest001, TestSize.Level2)
{
    ASSERT_EQ(static_cast<int>(CloseTrace()), static_cast<int>(TraceErrorCode::SUCCESS));
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    ASSERT_EQ(static_cast<int>(OpenTrace(appArgs)), static_cast<int>(TraceErrorCode::SUCCESS));
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();
    EXPECT_TRUE(traceDumpExecutor.PreCheckDumpTraceLoopStatus());
    TraceDumpParam param = {
        .type = TraceDumpType::TRACE_RECORDING
    };
    auto it = [&traceDumpExecutor](const TraceDumpParam& param) {
        EXPECT_TRUE(traceDumpExecutor.StartDumpTraceLoop(param));
    };
    std::thread traceLoopThread(it, param);
    sleep(3);
    auto list = traceDumpExecutor.StopDumpTraceLoop();
    EXPECT_GT(list.size(), 0);
    for (const auto& filename : list) {
        GTEST_LOG_(INFO) << filename;
        EXPECT_GT(GetFileSize(filename), 0);
    }
    traceLoopThread.join();
    ASSERT_EQ(static_cast<int>(CloseTrace()), static_cast<int>(TraceErrorCode::SUCCESS));
}

/**
 * @tc.name: TraceDumpExecutorTest002
 * @tc.desc: Test TraceDumpExecutor class StartDumpTraceLoop/StopDumpTraceLoop function.
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpExecutorTest002, TestSize.Level2)
{
    ASSERT_EQ(static_cast<int>(CloseTrace()), static_cast<int>(TraceErrorCode::SUCCESS));
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    ASSERT_EQ(static_cast<int>(OpenTrace(appArgs)), static_cast<int>(TraceErrorCode::SUCCESS));
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();
    EXPECT_TRUE(traceDumpExecutor.PreCheckDumpTraceLoopStatus());
    TraceDumpParam param = {
        .type = TraceDumpType::TRACE_RECORDING,
        .outputFile = TEST_TRACE_TEMP_FILE
    };
    auto it = [&traceDumpExecutor](const TraceDumpParam& param) {
        EXPECT_TRUE(traceDumpExecutor.StartDumpTraceLoop(param));
    };
    std::thread traceLoopThread(it, param);
    sleep(3);
    EXPECT_GT(traceDumpExecutor.StopDumpTraceLoop().size(), 0);
    traceLoopThread.join();
    EXPECT_GT(GetFileSize(TEST_TRACE_TEMP_FILE), 0);
    ASSERT_EQ(static_cast<int>(CloseTrace()), static_cast<int>(TraceErrorCode::SUCCESS));
    if (remove(TEST_TRACE_TEMP_FILE) != 0) {
        GTEST_LOG_(WARNING) << "Delete test trace file failed.";
    }
}

/**
 * @tc.name: TraceDumpExecutorTest003
 * @tc.desc: Test TraceDumpExecutor class DumpTrace function.
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpExecutorTest003, TestSize.Level2)
{
    ASSERT_EQ(static_cast<int>(CloseTrace()), static_cast<int>(TraceErrorCode::SUCCESS));
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    ASSERT_EQ(static_cast<int>(OpenTrace(appArgs)), static_cast<int>(TraceErrorCode::SUCCESS));
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();
    TraceDumpParam param;
    auto ret = traceDumpExecutor.DumpTrace(param);
    EXPECT_EQ(static_cast<int>(ret.code), static_cast<int>(TraceErrorCode::SUCCESS));
    GTEST_LOG_(INFO) << "snapshot file: " << ret.outputFile;
    EXPECT_GT(GetFileSize(ret.outputFile), 0);
    ASSERT_EQ(static_cast<int>(CloseTrace()), static_cast<int>(TraceErrorCode::SUCCESS));
}

/**
 * @tc.name: TraceDumpExecutorTest004
 * @tc.desc: Test TraceDumpExecutor class StartCacheTraceLoop/StopCacheTraceLoop function.
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpExecutorTest004, TestSize.Level2)
{
    ASSERT_EQ(static_cast<int>(CloseTrace()), static_cast<int>(TraceErrorCode::SUCCESS));
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    ASSERT_EQ(static_cast<int>(OpenTrace(appArgs)), static_cast<int>(TraceErrorCode::SUCCESS));
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();
    traceDumpExecutor.ClearCacheTraceFiles();
    EXPECT_TRUE(traceDumpExecutor.PreCheckDumpTraceLoopStatus());
    TraceDumpParam param = {
        .type = TraceDumpType::TRACE_CACHE,
        .cacheTotalFileSizeLmt = 50 * BYTE_PER_MB, // 50 : file size
        .cacheSliceDuration = 5 // 5 : slice
    };
    auto it = [&traceDumpExecutor](const TraceDumpParam& param) {
        EXPECT_TRUE(traceDumpExecutor.StartCacheTraceLoop(param));
    };
    std::thread traceLoopThread(it, param);
    sleep(8); // 8 : 8 seconds
    traceDumpExecutor.StopCacheTraceLoop();
    traceLoopThread.join();
    auto list = traceDumpExecutor.GetCacheTraceFiles();
    EXPECT_EQ(list.size(), 2); // 2 : should have 2 files
    for (const auto& file : list) {
        GTEST_LOG_(INFO) << file.filename;
        EXPECT_GT(GetFileSize(file.filename), 0);
    }
    ASSERT_EQ(static_cast<int>(CloseTrace()), static_cast<int>(TraceErrorCode::SUCCESS));
}

/**
 * @tc.name: TraceDumpExecutorTest005
 * @tc.desc: Test TraceDumpExecutor class StartCacheTraceLoop/StopCacheTraceLoop function.
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpExecutorTest005, TestSize.Level2)
{
    ASSERT_EQ(static_cast<int>(CloseTrace()), static_cast<int>(TraceErrorCode::SUCCESS));
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    ASSERT_EQ(static_cast<int>(OpenTrace(appArgs)), static_cast<int>(TraceErrorCode::SUCCESS));
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();
    traceDumpExecutor.ClearCacheTraceFiles();
    EXPECT_TRUE(traceDumpExecutor.PreCheckDumpTraceLoopStatus());
    TraceDumpParam param = {
        .type = TraceDumpType::TRACE_CACHE,
        .cacheTotalFileSizeLmt = 100 * BYTE_PER_MB, // 100 : file size
        .cacheSliceDuration = 5 // 5 : slice
    };
    auto it = [&traceDumpExecutor](const TraceDumpParam& param) {
        EXPECT_TRUE(traceDumpExecutor.StartCacheTraceLoop(param));
    };
    std::thread traceLoopThread(it, param);
    sleep(7);
    auto list1 = traceDumpExecutor.GetCacheTraceFiles();
    EXPECT_GE(list1.size(), 2); // 2 : should have 2 files
    for (const auto& file : list1) {
        GTEST_LOG_(INFO) << file.filename;
        EXPECT_GT(GetFileSize(file.filename), 0);
    }
    sleep(1);
    traceDumpExecutor.StopCacheTraceLoop();
    auto list2 = traceDumpExecutor.GetCacheTraceFiles();
    EXPECT_GE(list2.size(), 3); // 3 : should have 3 files
    for (const auto& file : list2) {
        GTEST_LOG_(INFO) << file.filename;
        EXPECT_GT(GetFileSize(file.filename), 0);
    }
    traceLoopThread.join();
    ASSERT_EQ(static_cast<int>(CloseTrace()), static_cast<int>(TraceErrorCode::SUCCESS));
}

/**
 * @tc.name: TraceDumpExecutorTest006
 * @tc.desc: Test TraceDumpExecutor class TraceDumpTaskMonitor function.
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpExecutorTest006, TestSize.Level2)
{
    ASSERT_EQ(static_cast<int>(CloseTrace()), static_cast<int>(TraceErrorCode::SUCCESS));
    std::string appArgs = "tags:sched,binder,ohos bufferSize:102400 overwrite:1";
    ASSERT_EQ(static_cast<int>(OpenTrace(appArgs)), static_cast<int>(TraceErrorCode::SUCCESS));
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    std::thread dumpThread([]() {
        auto& traceDumpExecutor = TraceDumpExecutor::GetInstance();
        std::thread loopReadThrad(&TraceDumpExecutor::ReadRawTraceLoop, std::ref(traceDumpExecutor));
        std::thread loopWriteThread(&TraceDumpExecutor::WriteTraceLoop, std::ref(traceDumpExecutor));
        traceDumpExecutor.TraceDumpTaskMonitor();
        loopReadThrad.join();
        loopWriteThread.join();
        GTEST_LOG_(INFO) << "TraceDumpExecutorTest006: dump thread exit.";
    });
    auto dumpPipe1 = std::make_shared<HitraceDumpPipe>(true);
    TraceDumpTask task = {
        .time = 1,
        .traceStartTime = 0,
        .traceEndTime = std::numeric_limits<uint64_t>::max()
    };
    EXPECT_TRUE(dumpPipe1->SubmitTraceDumpTask(task));
    sleep(1);
    EXPECT_TRUE(dumpPipe1->ReadSyncDumpRet(10, task)); // 10 : timeout
    EXPECT_EQ(static_cast<int>(task.code), static_cast<int>(TraceErrorCode::SUCCESS));
    EXPECT_TRUE(dumpPipe1->ReadAsyncDumpRet(10, task)); // 10 : timeout
    EXPECT_EQ(static_cast<int>(task.code), static_cast<int>(TraceErrorCode::SUCCESS));
    EXPECT_EQ(task.status, TraceDumpStatus::WRITE_DONE);
    dumpThread.join();
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_EQ(static_cast<int>(CloseTrace()), static_cast<int>(TraceErrorCode::SUCCESS));
}

/**
 * @tc.name: TraceDumpExecutorTest007
 * @tc.desc: Test TraceDumpExecutor with filtered pids.
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpExecutorTest007, TestSize.Level2)
{
    ASSERT_EQ(static_cast<int>(CloseTrace()), TraceErrorCode::SUCCESS);
    TraceArgs traceArgs = {
        .tags = { "sched" },
        .clockType = "boot",
        .isOverWrite = true,
        .appPid = 0,
        .filterPids = { getpid() }
    };
    ASSERT_EQ(static_cast<int>(OpenTrace(traceArgs)), TraceErrorCode::SUCCESS);
    ASSERT_EQ(static_cast<int>(TraceDumpExecutor::GetInstance().DumpTrace({}).code), TraceErrorCode::SUCCESS);
    ASSERT_EQ(static_cast<int>(CloseTrace()), TraceErrorCode::SUCCESS);
}

/**
 * @tc.name: TraceDumpPipeTest001
 * @tc.desc: Test TraceDumpExecutor class trace task pipe
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpPipeTest001, TestSize.Level2)
{
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    pid_t pid = fork();
    if (pid < 0) {
        FAIL() << "Failed to fork process.";
    } else if (pid == 0) {
        auto dumpPipe2 = std::make_shared<HitraceDumpPipe>(false);
        TraceDumpTask task;
        EXPECT_TRUE(dumpPipe2->ReadTraceTask(1000, task));
        EXPECT_EQ(task.status, TraceDumpStatus::READ_DONE);
        _exit(0);
    }
    auto dumpPipe1 = std::make_shared<HitraceDumpPipe>(true);
    TraceDumpTask task = {
        .status = TraceDumpStatus::READ_DONE,
    };
    EXPECT_TRUE(dumpPipe1->SubmitTraceDumpTask(task));
    waitpid(pid, nullptr, 0);
    HitraceDumpPipe::ClearTraceDumpPipe();
}

/**
 * @tc.name: TraceDumpPipeTest002
 * @tc.desc: Test TraceDumpExecutor class sync dump pipe
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpPipeTest002, TestSize.Level2)
{
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    pid_t pid = fork();
    if (pid < 0) {
        FAIL() << "Failed to fork process.";
    } else if (pid == 0) {
        auto dumpPipe2 = std::make_shared<HitraceDumpPipe>(false);
        TraceDumpTask task = {
            .status = TraceDumpStatus::READ_DONE,
        };
        EXPECT_TRUE(dumpPipe2->WriteSyncReturn(task));
        _exit(0);
    }
    auto dumpPipe1 = std::make_shared<HitraceDumpPipe>(true);
    TraceDumpTask task;
    EXPECT_TRUE(dumpPipe1->ReadSyncDumpRet(1, task));
    EXPECT_EQ(task.status, TraceDumpStatus::READ_DONE);
    waitpid(pid, nullptr, 0);
    HitraceDumpPipe::ClearTraceDumpPipe();
}

/**
 * @tc.name: TraceDumpPipeTest003
 * @tc.desc: Test TraceDumpExecutor class async dump pipe
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpPipeTest003, TestSize.Level2)
{
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    pid_t pid = fork();
    if (pid < 0) {
        FAIL() << "Failed to fork process.";
    } else if (pid == 0) {
        auto dumpPipe2 = std::make_shared<HitraceDumpPipe>(false);
        TraceDumpTask task = {
            .status = TraceDumpStatus::FINISH,
        };
        EXPECT_TRUE(dumpPipe2->WriteAsyncReturn(task));
        _exit(0);
    }
    auto dumpPipe1 = std::make_shared<HitraceDumpPipe>(true);
    TraceDumpTask task;
    EXPECT_TRUE(dumpPipe1->ReadAsyncDumpRet(1, task));
    EXPECT_EQ(task.status, TraceDumpStatus::FINISH);
    waitpid(pid, nullptr, 0);
    HitraceDumpPipe::ClearTraceDumpPipe();
}

/**
 * @tc.name: TraceDumpPipeTest004
 * @tc.desc: Test TraceDumpExecutor class trace task pipe
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpPipeTest004, TestSize.Level2)
{
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    pid_t pid = fork();
    if (pid < 0) {
        FAIL() << "Failed to fork process.";
    } else if (pid == 0) {
        auto dumpPipe2 = std::make_shared<HitraceDumpPipe>(false);
        TraceDumpTask task1;
        EXPECT_TRUE(dumpPipe2->ReadTraceTask(1000, task1));
        EXPECT_EQ(task1.status, TraceDumpStatus::READ_DONE);
        TraceDumpTask task2;
        EXPECT_TRUE(dumpPipe2->ReadTraceTask(1000, task2));
        EXPECT_EQ(task2.status, TraceDumpStatus::READ_DONE);
        _exit(0);
    }

    auto dumpPipe = std::make_shared<HitraceDumpPipe>(true);
    auto submitTask = [&dumpPipe]() {
        TraceDumpTask task = {
            .status = TraceDumpStatus::READ_DONE,
        };
        EXPECT_TRUE(dumpPipe->SubmitTraceDumpTask(task));
    };

    std::thread submitTaskThread(submitTask);
    std::thread submitTaskThread2(submitTask);
    submitTaskThread.join();
    submitTaskThread2.join();
    waitpid(pid, nullptr, 0);
    HitraceDumpPipe::ClearTraceDumpPipe();
}

/**
 * @tc.name: TraceDumpPipeTest005
 * @tc.desc: Test TraceDumpExecutor class trace task pipe exception branch
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpPipeTest005, TestSize.Level2)
{
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    std::thread childThread([]() {
        auto dumpPipe = std::make_shared<HitraceDumpPipe>(false);
        TraceDumpTask task = {
            .time = 1000, // 1000 : task id
            .status = TraceDumpStatus::READ_DONE
        };
        EXPECT_FALSE(dumpPipe->SubmitTraceDumpTask(task));
        EXPECT_FALSE(dumpPipe->ReadSyncDumpRet(1000, task)); // 1000 : timeout
        EXPECT_FALSE(dumpPipe->ReadAsyncDumpRet(1000, task)); // 1000 : timeout
        GTEST_LOG_(INFO) << "TraceDumpPipeTest005: child thread exit.";
    });
    auto dumpPipe = std::make_shared<HitraceDumpPipe>(true);
    TraceDumpTask task;
    EXPECT_FALSE(dumpPipe->ReadTraceTask(1000, task)); // 1000 : timeout
    EXPECT_FALSE(dumpPipe->WriteSyncReturn(task));
    EXPECT_FALSE(dumpPipe->WriteAsyncReturn(task));
    childThread.join();
    HitraceDumpPipe::ClearTraceDumpPipe();
}

/**
 * @tc.name: TraceDumpPipeTest006
 * @tc.desc: Test TraceDumpExecutor class trace task pipe exception branch
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpPipeTest006, TestSize.Level2)
{
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    unlink("/data/log/hitrace/trace_task");
    std::thread childThread([]() {
        auto dumpPipe = std::make_shared<HitraceDumpPipe>(false);
        TraceDumpTask task;
        EXPECT_FALSE(dumpPipe->ReadTraceTask(1000, task)); // 1000 : timeout
        GTEST_LOG_(INFO) << "TraceDumpPipeTest006: child thread exit.";
    });
    auto dumpPipe = std::make_shared<HitraceDumpPipe>(true);
    TraceDumpTask task = {
        .time = 1000, // 1000 : task id
        .status = TraceDumpStatus::READ_DONE
    };
    EXPECT_FALSE(dumpPipe->SubmitTraceDumpTask(task));
    childThread.join();
    HitraceDumpPipe::ClearTraceDumpPipe();
}

/**
 * @tc.name: TraceDumpPipeTest007
 * @tc.desc: Test TraceDumpExecutor class trace task pipe exception branch
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpPipeTest007, TestSize.Level2)
{
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    unlink("/data/log/hitrace/trace_sync_return");
    std::thread childThread([]() {
        auto dumpPipe = std::make_shared<HitraceDumpPipe>(false);
        TraceDumpTask task = {
            .time = 1000, // 1000 : task id
            .status = TraceDumpStatus::READ_DONE
        };
        EXPECT_FALSE(dumpPipe->WriteSyncReturn(task));
        GTEST_LOG_(INFO) << "TraceDumpPipeTest007: child thread exit.";
    });
    auto dumpPipe = std::make_shared<HitraceDumpPipe>(true);
    TraceDumpTask task;
    EXPECT_FALSE(dumpPipe->ReadSyncDumpRet(1000, task)); // 1000 : timeout
    childThread.join();
    HitraceDumpPipe::ClearTraceDumpPipe();
}

/**
 * @tc.name: TraceDumpPipeTest008
 * @tc.desc: Test TraceDumpExecutor class trace task pipe exception branch
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpPipeTest008, TestSize.Level2)
{
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    unlink("/data/log/hitrace/trace_async_return");
    std::thread childThread([]() {
        auto dumpPipe = std::make_shared<HitraceDumpPipe>(false);
        TraceDumpTask task = {
            .time = 1000, // 1000 : task id
            .status = TraceDumpStatus::READ_DONE
        };
        EXPECT_FALSE(dumpPipe->WriteAsyncReturn(task));
        GTEST_LOG_(INFO) << "TraceDumpPipeTest008: child thread exit.";
    });
    auto dumpPipe = std::make_shared<HitraceDumpPipe>(true);
    TraceDumpTask task;
    EXPECT_FALSE(dumpPipe->ReadAsyncDumpRet(1000, task)); // 1000 : timeout
    childThread.join();
    HitraceDumpPipe::ClearTraceDumpPipe();
}

/**
 * @tc.name: TraceDumpTaskTest001
 * @tc.desc: Test TraceDumpExecutor class task management functions
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpTaskTest001, TestSize.Level2)
{
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();

    // Test IsTraceDumpTaskEmpty and GetTraceDumpTaskCount when empty
    EXPECT_TRUE(traceDumpExecutor.IsTraceDumpTaskEmpty());
    EXPECT_EQ(traceDumpExecutor.GetTraceDumpTaskCount(), 0);

    // Test AddTraceDumpTask
    TraceDumpTask task1 = {
        .time = 1000,
        .status = TraceDumpStatus::START,
        .code = TraceErrorCode::UNSET,
    };
    traceDumpExecutor.AddTraceDumpTask(task1);
    EXPECT_FALSE(traceDumpExecutor.IsTraceDumpTaskEmpty());
    EXPECT_EQ(traceDumpExecutor.GetTraceDumpTaskCount(), 1);

    // Test UpdateTraceDumpTask
    task1.status = TraceDumpStatus::READ_DONE;
    task1.code = TraceErrorCode::SUCCESS;
    EXPECT_TRUE(traceDumpExecutor.UpdateTraceDumpTask(task1));

    // Test RemoveTraceDumpTask
    traceDumpExecutor.RemoveTraceDumpTask(task1.time);
    EXPECT_TRUE(traceDumpExecutor.IsTraceDumpTaskEmpty());
    EXPECT_EQ(traceDumpExecutor.GetTraceDumpTaskCount(), 0);
}

/**
 * @tc.name: TraceDumpTaskTest002
 * @tc.desc: Test TraceDumpExecutor class multiple tasks management
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpTaskTest002, TestSize.Level2)
{
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();

    // Add multiple tasks
    TraceDumpTask task1 = {
        .time = 1000,
        .status = TraceDumpStatus::START,
        .code = TraceErrorCode::UNSET,
    };
    TraceDumpTask task2 = {
        .time = 2000,
        .status = TraceDumpStatus::START,
        .code = TraceErrorCode::UNSET,
    };

    traceDumpExecutor.AddTraceDumpTask(task1);
    traceDumpExecutor.AddTraceDumpTask(task2);
    EXPECT_EQ(traceDumpExecutor.GetTraceDumpTaskCount(), 2);

    // Update first task
    task1.status = TraceDumpStatus::READ_DONE;
    EXPECT_TRUE(traceDumpExecutor.UpdateTraceDumpTask(task1));

    // Remove first task
    traceDumpExecutor.RemoveTraceDumpTask(task1.time);
    EXPECT_EQ(traceDumpExecutor.GetTraceDumpTaskCount(), 1);

    // Remove second task
    traceDumpExecutor.RemoveTraceDumpTask(task2.time);
    EXPECT_TRUE(traceDumpExecutor.IsTraceDumpTaskEmpty());
}

/**
 * @tc.name: TraceDumpTaskTest003
 * @tc.desc: Test TraceDumpExecutor class task update with invalid data
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpTaskTest003, TestSize.Level2)
{
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();

    // Add a task
    TraceDumpTask task = {
        .time = 1000,
        .status = TraceDumpStatus::START,
        .code = TraceErrorCode::UNSET,
    };
    traceDumpExecutor.AddTraceDumpTask(task);

    // Try to update non-existent task
    TraceDumpTask invalidTask = {
        .time = 9999,
        .status = TraceDumpStatus::READ_DONE,
        .code = TraceErrorCode::SUCCESS,
    };
    EXPECT_FALSE(traceDumpExecutor.UpdateTraceDumpTask(invalidTask));

    // Remove non-existent task
    traceDumpExecutor.RemoveTraceDumpTask(9999);
    EXPECT_EQ(traceDumpExecutor.GetTraceDumpTaskCount(), 1);

    // Clean up
    traceDumpExecutor.RemoveTraceDumpTask(task.time);
    EXPECT_TRUE(traceDumpExecutor.IsTraceDumpTaskEmpty());
}

/**
 * @tc.name: TraceDumpTaskTest004
 * @tc.desc: Test TraceDumpExecutor class
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpTaskTest004, TestSize.Level2)
{
    TraceDumpExecutor& traceDumpExecutor = TraceDumpExecutor::GetInstance();
    // Add a task
    TraceDumpTask task = {
        .time = 1000,
        .status = TraceDumpStatus::START,
        .code = TraceErrorCode::UNSET,
    };
    traceDumpExecutor.AddTraceDumpTask(task);

    TraceDumpTask task2 = {
        .time = 9999,
        .status = TraceDumpStatus::READ_DONE,
        .code = TraceErrorCode::SUCCESS,
    };
    traceDumpExecutor.AddTraceDumpTask(task2);

    traceDumpExecutor.ClearTraceDumpTask();
    EXPECT_TRUE(traceDumpExecutor.IsTraceDumpTaskEmpty());
    EXPECT_EQ(traceDumpExecutor.GetTraceDumpTaskCount(), 0);
}

/**
 * @tc.name: TraceDumpTaskTest005
 * @tc.desc: Test DoProcessTraceDumpTask async return when task status is WRITE_DONE
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpTaskTest005, TestSize.Level2)
{
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    std::thread childThread([]() {
        auto dumpPipe = std::make_shared<HitraceDumpPipe>(false);
        TraceDumpTask task = {
            .time = GetCurBootTime(),
            .hasSyncReturn = true,
            .writeRetry = 0,
            .code = TraceErrorCode::UNSET,
            .status = TraceDumpStatus::WRITE_DONE
        };
        std::vector<TraceDumpTask> completedTasks;
        TraceDumpExecutor::GetInstance().DoProcessTraceDumpTask(dumpPipe, task, completedTasks);
        EXPECT_EQ(completedTasks.size(), 1);
        GTEST_LOG_(INFO) << "TraceDumpTaskTest005: child thread exit.";
    });
    auto dumpPipe = std::make_shared<HitraceDumpPipe>(true);
    TraceDumpTask task;
    EXPECT_TRUE(dumpPipe->ReadAsyncDumpRet(TIMEOUT_5S, task));
    childThread.join();
    HitraceDumpPipe::ClearTraceDumpPipe();
}

/**
 * @tc.name: TraceDumpTaskTest006
 * @tc.desc: Test DoProcessTraceDumpTask sync return when task status is WRITE_DONE
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpTaskTest006, TestSize.Level2)
{
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    std::thread childThread([]() {
        auto dumpPipe = std::make_shared<HitraceDumpPipe>(false);
        TraceDumpTask task = {
            .time = GetCurBootTime(),
            .hasSyncReturn = false,
            .writeRetry = 0,
            .code = TraceErrorCode::UNSET,
            .status = TraceDumpStatus::WRITE_DONE
        };
        std::vector<TraceDumpTask> completedTasks;
        TraceDumpExecutor::GetInstance().DoProcessTraceDumpTask(dumpPipe, task, completedTasks);
        EXPECT_EQ(completedTasks.size(), 1);
        EXPECT_TRUE(task.hasSyncReturn);

        task.time -= SYNC_RETURN_TIMEOUT_NS;
        task.hasSyncReturn = false;
        completedTasks.clear();
        TraceDumpExecutor::GetInstance().DoProcessTraceDumpTask(dumpPipe, task, completedTasks);
        EXPECT_EQ(completedTasks.size(), 0);
        EXPECT_TRUE(task.hasSyncReturn);
        GTEST_LOG_(INFO) << "TraceDumpTaskTest006: child thread exit.";
    });
    auto dumpPipe = std::make_shared<HitraceDumpPipe>(true);
    TraceDumpTask task;
    EXPECT_TRUE(dumpPipe->ReadSyncDumpRet(TIMEOUT_5S, task));
    EXPECT_TRUE(dumpPipe->ReadSyncDumpRet(TIMEOUT_5S, task));
    childThread.join();
    HitraceDumpPipe::ClearTraceDumpPipe();
}

/**
 * @tc.name: TraceDumpTaskTest007
 * @tc.desc: Test DoProcessTraceDumpTask async return when task status is READ_DONE
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpTaskTest007, TestSize.Level2)
{
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    std::thread childThread([]() {
        auto dumpPipe = std::make_shared<HitraceDumpPipe>(false);
        TraceDumpTask task = {
            .time = GetCurBootTime(),
            .hasSyncReturn = true,
            .writeRetry = 0,
            .code = TraceErrorCode::UNSET,
            .status = TraceDumpStatus::READ_DONE
        };
        std::vector<TraceDumpTask> completedTasks;
        TraceDumpExecutor::GetInstance().DoProcessTraceDumpTask(dumpPipe, task, completedTasks);
        EXPECT_EQ(completedTasks.size(), 1);
        GTEST_LOG_(INFO) << "TraceDumpTaskTest007: child thread exit.";
    });
    auto dumpPipe = std::make_shared<HitraceDumpPipe>(true);
    TraceDumpTask task;
    EXPECT_TRUE(dumpPipe->ReadAsyncDumpRet(1000, task)); // 1000 : timeout
    EXPECT_EQ(task.status, TraceDumpStatus::WRITE_DONE);
    childThread.join();
    HitraceDumpPipe::ClearTraceDumpPipe();
}

/**
 * @tc.name: TraceDumpTaskTest008
 * @tc.desc: Test DoProcessTraceDumpTask sync return when task status is READ_DONE
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpTaskTest008, TestSize.Level2)
{
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    std::thread childThread([]() {
        auto dumpPipe = std::make_shared<HitraceDumpPipe>(false);
        TraceDumpTask task = {
            .time = GetCurBootTime(),
            .hasSyncReturn = false,
            .writeRetry = 0,
            .code = TraceErrorCode::UNSET,
            .status = TraceDumpStatus::READ_DONE
        };
        std::vector<TraceDumpTask> completedTasks;
        TraceDumpExecutor::GetInstance().DoProcessTraceDumpTask(dumpPipe, task, completedTasks);
        EXPECT_EQ(completedTasks.size(), 1);
        GTEST_LOG_(INFO) << "TraceDumpTaskTest008: child thread exit.";
    });
    auto dumpPipe = std::make_shared<HitraceDumpPipe>(true);
    TraceDumpTask task;
    EXPECT_TRUE(dumpPipe->ReadSyncDumpRet(TIMEOUT_5S, task));
    EXPECT_EQ(task.status, TraceDumpStatus::WRITE_DONE);
    childThread.join();
    HitraceDumpPipe::ClearTraceDumpPipe();
}

/**
 * @tc.name: TraceDumpTaskTest009
 * @tc.desc: Test DoProcessTraceDumpTask retry 10 times
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpTaskTest009, TestSize.Level2)
{
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    std::thread childThread([]() {
        auto dumpPipe = std::make_shared<HitraceDumpPipe>(false);
        TraceDumpTask task = {
            .time = GetCurBootTime(),
            .hasSyncReturn = false,
            .writeRetry = 0,
            .code = TraceErrorCode::UNSET,
            .status = TraceDumpStatus::START
        };
        std::vector<TraceDumpTask> completedTasks;
        TraceDumpExecutor::GetInstance().DoProcessTraceDumpTask(dumpPipe, task, completedTasks);
        EXPECT_EQ(completedTasks.size(), 0);
        task.writeRetry = 10; // retry 10 times
        TraceDumpExecutor::GetInstance().DoProcessTraceDumpTask(dumpPipe, task, completedTasks);
        EXPECT_EQ(completedTasks.size(), 1);
        GTEST_LOG_(INFO) << "TraceDumpTaskTest009: child thread exit.";
    });
    auto dumpPipe = std::make_shared<HitraceDumpPipe>(true);
    TraceDumpTask task;
    EXPECT_FALSE(dumpPipe->ReadSyncDumpRet(TIMEOUT_5S, task));
    childThread.join();
    HitraceDumpPipe::ClearTraceDumpPipe();
}

/**
 * @tc.name: TraceDumpTaskTest010
 * @tc.desc: Test DoProcessTraceDumpTask sync return timeout
 * @tc.type: FUNC
 */
HWTEST_F(TraceDumpExecutorTest, TraceDumpTaskTest010, TestSize.Level2)
{
    HitraceDumpPipe::ClearTraceDumpPipe();
    ASSERT_TRUE(HitraceDumpPipe::InitTraceDumpPipe());
    std::thread childThread([]() {
        auto dumpPipe = std::make_shared<HitraceDumpPipe>(false);
        TraceDumpTask task = {
            .time = GetCurBootTime(),
            .hasSyncReturn = true,
            .writeRetry = 0,
            .code = TraceErrorCode::SUCCESS,
            .status = TraceDumpStatus::READ_DONE
        };
        std::vector<TraceDumpTask> completedTasks;
        TraceDumpExecutor::GetInstance().DoProcessTraceDumpTask(dumpPipe, task, completedTasks);
        EXPECT_EQ(completedTasks.size(), 0);
        EXPECT_TRUE(task.hasSyncReturn);
        EXPECT_EQ(task.status, TraceDumpStatus::READ_DONE);

        task.hasSyncReturn = false;
        TraceDumpExecutor::GetInstance().DoProcessTraceDumpTask(dumpPipe, task, completedTasks);
        EXPECT_EQ(completedTasks.size(), 0);
        EXPECT_FALSE(task.hasSyncReturn);
        EXPECT_EQ(task.status, TraceDumpStatus::READ_DONE);

        task.time -=  - SYNC_RETURN_TIMEOUT_NS;
        TraceDumpExecutor::GetInstance().DoProcessTraceDumpTask(dumpPipe, task, completedTasks);
        EXPECT_EQ(completedTasks.size(), 0);
        EXPECT_TRUE(task.hasSyncReturn);
        EXPECT_EQ(task.status, TraceDumpStatus::WAIT_WRITE);
        GTEST_LOG_(INFO) << "TraceDumpTaskTest010: child thread exit.";
    });
    auto dumpPipe = std::make_shared<HitraceDumpPipe>(true);
    TraceDumpTask task;
    EXPECT_TRUE(dumpPipe->ReadSyncDumpRet(TIMEOUT_5S, task));
    childThread.join();
    HitraceDumpPipe::ClearTraceDumpPipe();
}

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

class FailingSourceFactory : public TraceSourceLinuxFactory {
public:
    explicit FailingSourceFactory(const std::string& path, bool failInitialization = false, bool isUserTrace = false)
        : TraceSourceLinuxFactory(path, isUserTrace), failInitialization_(failInitialization) {}

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
    bool NeedDoPreAndPost() const override
    {
        return false;
    }

    bool NeedCreateTraceContentPtr() const override
    {
        return false;
    }
};

class CacheFailureStrategy : public CacheTraceDumpStrategy {
protected:
    bool NeedDoPreAndPost() const override
    {
        return false;
    }
};

class RecordFailureStrategy : public RecordTraceDumpStrategy {
protected:
    bool NeedDoPreAndPost() const override
    {
        return false;
    }
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

    void CheckSystemFileAgeing(const std::string& file)
    {
        ExpectSource(file, "system");
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

protected:
    std::string directory_;
    std::string externalDirectory_;
};

HWTEST_F(TraceSourcePolicyTest, AsyncWriteFailureRetainsSystemSource, TestSize.Level1)
{
    const std::string file = directory_ + "/trace_failed.sys";
    auto factory = std::make_shared<FailingSourceFactory>(file);
    const TraceDumpRequest request {
        .type = TraceDumpType::TRACE_ASYNC_WRITE
    };
    const auto ret = AsyncFailureStrategy().Execute(factory, request);
    EXPECT_EQ(ret.code, TraceErrorCode::WRITE_TRACE_INFO_ERROR);
    EXPECT_GT(GetFileSize(file), 0);
    ExpectSource(file, "system");
    const std::string replacement = directory_ + "/trace_replaced.sys";
    ASSERT_TRUE(factory->UpdateTraceFile(replacement));
    EXPECT_EQ(AsyncFailureStrategy().Execute(factory, request).code, TraceErrorCode::WRITE_TRACE_INFO_ERROR);
    EXPECT_GT(GetFileSize(replacement), 0);
    factory.reset();
    CheckSystemFileAgeing(file);
    CheckSystemFileAgeing(replacement);
}

HWTEST_F(TraceSourcePolicyTest, InitializationFailureRetainsSystemSource, TestSize.Level1)
{
    const std::string file = directory_ + "/trace_empty.sys";
    auto factory = std::make_shared<FailingSourceFactory>(file, true);
    const TraceDumpRequest request {};
    const auto ret = SnapshotTraceDumpStrategy().Execute(factory, request);
    EXPECT_EQ(ret.code, TraceErrorCode::WRITE_TRACE_INFO_ERROR);
    EXPECT_EQ(GetFileSize(file), 0);
    factory.reset();
    CheckSystemFileAgeing(file);
}

HWTEST_F(TraceSourcePolicyTest, CacheWriteFailureRetainsSystemSource, TestSize.Level1)
{
    const std::string file = directory_ + "/cache_trace_failed.sys";
    auto factory = std::make_shared<FailingSourceFactory>(file);
    const TraceDumpRequest request {
        .type = TraceDumpType::TRACE_CACHE
    };
    ASSERT_TRUE(TraceDumpState::GetInstance().StartLoopDump());
    CacheFailureStrategy().Execute(factory, request);
    TraceDumpState::GetInstance().EndLoopDumpSelf();
    EXPECT_GT(GetFileSize(file), 0);
    factory.reset();
    CheckSystemFileAgeing(file);
}

HWTEST_F(TraceSourcePolicyTest, FactoriesRespectSourceAndOutputDirectory, TestSize.Level1)
{
    int index = 0;
    for (const bool isUserTrace : {false, true}) {
        for (const bool hm : {false, true}) {
            for (const auto& directory : {directory_, externalDirectory_}) {
                const std::string name = "/trace_" + std::to_string(index++);
                const std::string file = directory + name;
                std::shared_ptr<ITraceSourceFactory> factory;
                if (hm) {
                    factory = std::make_shared<TraceSourceHMFactory>(file, isUserTrace);
                } else {
                    factory = std::make_shared<TraceSourceLinuxFactory>(file, isUserTrace);
                }
                const std::string expected = isUserTrace ? "user" : "system";
                if (directory == directory_) {
                    ExpectSource(file, expected);
                } else {
                    ExpectNoSource(file);
                }
                const std::string replacement = directory_ + name + "_replacement";
                ASSERT_TRUE(factory->UpdateTraceFile(replacement));
                ExpectSource(replacement, expected);
                const std::string externalFile = externalDirectory_ + name + "_replacement";
                ASSERT_TRUE(factory->UpdateTraceFile(externalFile));
                ExpectNoSource(externalFile);
            }
        }
    }
}

HWTEST_F(TraceSourcePolicyTest, UserInitializationFailureRemainsExcludedFromSystemAgeing, TestSize.Level1)
{
    const std::string file = directory_ + "/trace_user_failed.sys";
    auto factory = std::make_shared<FailingSourceFactory>(file, true, true);
    const TraceDumpRequest request {};
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
    auto factory = std::make_shared<FailingSourceFactory>(file, false, true);
    const TraceDumpRequest request {
        .type = TraceDumpType::TRACE_RECORDING
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
    auto factory = std::make_shared<TraceSourceLinuxFactory>(output);
    const TraceDumpRequest request {};
    EXPECT_EQ(SnapshotTraceDumpStrategy().Execute(factory, request).code, TraceErrorCode::WRITE_TRACE_INFO_ERROR);
    struct stat info {};
    ASSERT_EQ(stat(output.c_str(), &info), 0);
    EXPECT_TRUE(S_ISDIR(info.st_mode));
    EXPECT_EQ(rmdir(output.c_str()), 0);
}
} // namespace
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS