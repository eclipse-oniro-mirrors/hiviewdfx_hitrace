/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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

#include "common_utils.h"

#include <cinttypes>
#include <unistd.h>
#include <cstdio>
#include <fstream>
#include <fcntl.h>
#include <sstream>
#include <sys/utsname.h>

#include "common_define.h"
#include "hilog/log.h"
#include "parameters.h"
#include "securec.h"

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
const std::string CPUFREQ_PREFIX = "/sys/devices/system/cpu/cpu";
const std::string CPUFREQ_AFTERFIX = "/cpufreq/scaling_cur_freq";
}

std::string CanonicalizeSpecPath(const char* src)
{
    if (src == nullptr || strlen(src) >= PATH_MAX) {
        HILOG_ERROR(LOG_CORE, "CanonicalizeSpecPath: %{pubilc}s failed.", src);
        return "";
    }
    char resolvedPath[PATH_MAX] = { 0 };

    if (access(src, F_OK) == 0) {
        if (realpath(src, resolvedPath) == nullptr) {
            HILOG_ERROR(LOG_CORE, "CanonicalizeSpecPath: realpath %{pubilc}s failed.", src);
            return "";
        }
    } else {
        std::string fileName(src);
        if (fileName.find("..") == std::string::npos) {
            if (sprintf_s(resolvedPath, PATH_MAX, "%s", src) == -1) {
                HILOG_ERROR(LOG_CORE, "CanonicalizeSpecPath: sprintf_s %{pubilc}s failed.", src);
                return "";
            }
        } else {
            HILOG_ERROR(LOG_CORE, "CanonicalizeSpecPath: find .. src failed.");
            return "";
        }
    }

    std::string res(resolvedPath);
    return res;
}

bool MarkClockSync(const std::string& traceRootPath)
{
    constexpr unsigned int bufferSize = 128;
    char buffer[bufferSize] = { 0 };
    std::string resolvedPath = CanonicalizeSpecPath((traceRootPath + TRACE_MARKER_NODE).c_str());
    int fd = open(resolvedPath.c_str(), O_WRONLY);
    if (fd == -1) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: oepn %{public}s fail, errno(%{public}d)", resolvedPath.c_str(), errno);
        return false;
    }

    // write realtime_ts
    struct timespec rts = {0, 0};
    if (clock_gettime(CLOCK_REALTIME, &rts) == -1) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: get realtime error, errno(%{public}d)", errno);
        close(fd);
        return false;
    }
    constexpr unsigned int nanoSeconds = 1000000000; // seconds converted to nanoseconds
    constexpr unsigned int nanoToMill = 1000000; // millisecond converted to nanoseconds
    int len = snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1,
        "trace_event_clock_sync: realtime_ts=%" PRId64 "\n",
        static_cast<int64_t>((rts.tv_sec * nanoSeconds + rts.tv_nsec) / nanoToMill));
    if (len < 0) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: entering realtime_ts into buffer error, errno(%{public}d)", errno);
        close(fd);
        return false;
    }

    if (write(fd, buffer, len) < 0) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: writing realtime error, errno(%{public}d)", errno);
    }

    // write parent_ts
    struct timespec mts = {0, 0};
    if (clock_gettime(CLOCK_MONOTONIC, &mts) == -1) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: get parent_ts error, errno(%{public}d)", errno);
        close(fd);
        return false;
    }
    constexpr float nanoToSecond = 1000000000.0f; // consistent with the ftrace timestamp format
    len = snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, "trace_event_clock_sync: parent_ts=%f\n",
        static_cast<float>(((static_cast<float>(mts.tv_sec)) * nanoSeconds + mts.tv_nsec) / nanoToSecond));
    if (len < 0) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: entering parent_ts into buffer error, errno(%{public}d)", errno);
        close(fd);
        return false;
    }
    if (write(fd, buffer, len) < 0) {
        HILOG_ERROR(LOG_CORE, "MarkClockSync: writing parent_ts error, errno(%{public}d)", errno);
    }
    close(fd);
    return true;
}

bool IsNumber(const std::string &str)
{
    if (str.empty()) {
        return false;
    }
    for (auto c : str) {
        if (!isdigit(c)) {
            return false;
        }
    }
    return true;
}

int GetCpuProcessors()
{
    int processors = 0;
    processors = sysconf(_SC_NPROCESSORS_CONF);
    return (processors == 0) ? 1 : processors;
}

void ReadCurrentCpuFrequencies(std::string& freqs)
{
    int cpuNum = GetCpuProcessors();
    std::ifstream file;
    std::string line;
    for (int i = 0; i < cpuNum; ++i) {
        std::string freq = "0";
        std::string cpuFreqPath = CPUFREQ_PREFIX + std::to_string(i) + CPUFREQ_AFTERFIX;
        file.open(cpuFreqPath);
        if (file.is_open()) {
            if (std::getline(file, line)) {
                std::istringstream iss(line);
                iss >> freq;
            }
            file.close();
        }
        freqs += "cpu_id=" + std::to_string(i) + " state=" + freq;
        if (i != cpuNum - 1) {
            freqs += ",";
        }
    }
}

bool SetPropertyInner(const std::string& property, const std::string& value)
{
    bool result = OHOS::system::SetParameter(property, value);
    if (!result) {
        fprintf(stderr, "Error: Failed to set %s property.\n", property.c_str());
    }
    return result;
}

std::string GetPropertyInner(const std::string& property, const std::string& value)
{
    return OHOS::system::GetParameter(property, value);
}

bool IsHmKernel()
{
    bool isHM = false;
    utsname unameBuf;
    if ((uname(&unameBuf)) == 0) {
        std::string osRelease = unameBuf.release;
        isHM = osRelease.find("HongMeng") != std::string::npos;
    }
    return isHM;
}

bool IsDeveloperMode()
{
    return OHOS::system::GetBoolParameter("const.security.developermode.state", false);
}

bool IsRootVersion()
{
    return OHOS::system::GetBoolParameter("const.debuggable", false);
}
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS