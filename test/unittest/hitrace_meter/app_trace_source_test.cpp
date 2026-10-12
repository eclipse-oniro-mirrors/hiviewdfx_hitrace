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
#include <gtest/gtest.h>
#include <string>
#include <sys/xattr.h>
#include <unistd.h>

#include "hitrace_meter.h"

using namespace testing::ext;

namespace OHOS::HiviewDFX::HitraceTest {
class AppTraceSourceTest : public testing::Test {
public:
    void SetUp() override
    {
        StopCaptureAppTrace();
        char directory[] = "/data/local/tmp/app_trace_source_XXXXXX";
        ASSERT_NE(mkdtemp(directory), nullptr);
        directory_ = directory;
    }

    void TearDown() override
    {
        StopCaptureAppTrace();
        if (!outputFile_.empty() && access(outputFile_.c_str(), F_OK) == 0) {
            EXPECT_EQ(remove(outputFile_.c_str()), 0);
        }
        if (!directory_.empty()) {
            EXPECT_EQ(rmdir(directory_.c_str()), 0);
        }
    }

    void CheckUserSource()
    {
        constexpr char expected[] = "user";
        char source[16] = {};
        ASSERT_EQ(getxattr(outputFile_.c_str(), "user.trace_source", source, sizeof(source)),
            static_cast<ssize_t>(sizeof(expected) - 1));
        EXPECT_STREQ(source, expected);
    }

protected:
    std::string directory_;
    std::string outputFile_;
};

HWTEST_F(AppTraceSourceTest, ExplicitAppTraceFileUsesUserSource, TestSize.Level1)
{
    constexpr uint64_t fileSize = 1024 * 1024;
    outputFile_ = directory_ + "/capture.trace";
    ASSERT_EQ(StartCaptureAppTrace(FLAG_MAIN_THREAD, HITRACE_TAG_OHOS, fileSize, outputFile_), RetType::RET_SUCC);
    CheckUserSource();
    EXPECT_EQ(StopCaptureAppTrace(), RetType::RET_SUCC);
    CheckUserSource();
}

HWTEST_F(AppTraceSourceTest, DefaultAppTraceFileUsesUserSource, TestSize.Level1)
{
    constexpr uint64_t fileSize = 1024 * 1024;
    std::string returnedPath;
    ASSERT_EQ(StartCaptureAppTrace(FLAG_ALL_THREAD, HITRACE_TAG_OHOS, fileSize, returnedPath), RetType::RET_SUCC);
    const auto separator = returnedPath.find_last_of('/');
    ASSERT_NE(separator, std::string::npos);
    outputFile_ = "/data/local/tmp/" + returnedPath.substr(separator + 1);
    CheckUserSource();
    EXPECT_EQ(StopCaptureAppTrace(), RetType::RET_SUCC);
    CheckUserSource();
}
} // namespace OHOS::HiviewDFX::HitraceTest
