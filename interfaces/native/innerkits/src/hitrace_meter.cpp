/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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

#include <atomic>
#include <cinttypes>
#include <climits>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <mutex>
#include <unistd.h>
#include <vector>
#include "securec.h"
#include "hilog/log.h"
#include "parameter.h"
#include "parameters.h"
#include "hitrace_meter.h"

using namespace std;
using namespace OHOS::HiviewDFX;

#define EXPECTANTLY(exp) (__builtin_expect(!!(exp), true))
#define UNEXPECTANTLY(exp) (__builtin_expect(!!(exp), false))
#define UNUSED_PARAM __attribute__((__unused__))

namespace {
int g_markerFd = -1;
std::once_flag g_onceFlag;

std::atomic<bool> g_isHitraceMeterDisabled(false);
std::atomic<bool> g_isHitraceMeterInit(false);
std::atomic<uint64_t> g_tagsProperty(HITRACE_TAG_NOT_READY);

const std::string KEY_TRACE_TAG = "debug.hitrace.tags.enableflags";
const std::string KEY_APP_NUMBER = "debug.hitrace.app_number";
const std::string KEY_RO_DEBUGGABLE = "ro.debuggable";
const std::string KEY_PREFIX = "debug.hitrace.app_";

constexpr int NAME_MAX_SIZE = 1000;
constexpr int VAR_NAME_MAX_SIZE = 256;
constexpr int NAME_NORMAL_LEN = 200;

static const int PID_BUF_SIZE = 6;
static char g_pid[PID_BUF_SIZE];
static const std::string EMPTY_TRACE_NAME;

static std::vector<std::string> g_markTypes = {"B", "E", "S", "F", "C"};
enum MarkerType { MARKER_BEGIN, MARKER_END, MARKER_ASYNC_BEGIN, MARKER_ASYNC_END, MARKER_INT, MARKER_MAX };

constexpr uint64_t HITRACE_TAG = 0xD002D33;
constexpr OHOS::HiviewDFX::HiLogLabel LABEL = {LOG_CORE, HITRACE_TAG, "HitraceMeter"};

static void ParameterChange(const char* key, const char* value, void* context)
{
    HiLog::Info(LABEL, "ParameterChange enableflags %{public}s", value);
    UpdateTraceLabel();
}

bool IsAppValid()
{
    // Judge if application-level tracing is enabled.
    if (OHOS::system::GetBoolParameter(KEY_RO_DEBUGGABLE, 0)) {
        std::ifstream fs;
        fs.open("/proc/self/cmdline");
        if (!fs.is_open()) {
            fprintf(stderr, "IsAppValid, open /proc/self/cmdline failed.\n");
            return false;
        }

        std::string lineStr;
        std::getline(fs, lineStr);
        int nums = OHOS::system::GetIntParameter<int>(KEY_APP_NUMBER, 0);
        for (int i = 0; i < nums; i++) {
            std::string keyStr = KEY_PREFIX + std::to_string(i);
            std::string val = OHOS::system::GetParameter(keyStr, "");
            if (val == "*" || val == lineStr) {
                fs.close();
                return true;
            }
        }
    }
    return false;
}

uint64_t GetSysParamTags()
{
    // Get the system parameters of KEY_TRACE_TAG.
    uint64_t tags = OHOS::system::GetUintParameter<uint64_t>(KEY_TRACE_TAG, 0);
    if (tags == 0) {
        // HiLog::Error(LABEL, "GetUintParameter %s error.\n", KEY_TRACE_TAG.c_str());
        return 0;
    }

    IsAppValid();
    return (tags | HITRACE_TAG_ALWAYS) & HITRACE_TAG_VALID_MASK;
}

// open file "trace_marker".
void OpenTraceMarkerFile()
{
    const std::string debugFile = "/sys/kernel/debug/tracing/trace_marker";
    const std::string traceFile = "/sys/kernel/tracing/trace_marker";
    g_markerFd = open(debugFile.c_str(), O_WRONLY | O_CLOEXEC);
    if (g_markerFd == -1) {
        HiLog::Error(LABEL, "open trace file %{public}s failed: %{public}d", debugFile.c_str(), errno);
        g_markerFd = open(traceFile.c_str(), O_WRONLY | O_CLOEXEC);
        if (g_markerFd == -1) {
            HiLog::Error(LABEL, "open trace file %{public}s failed: %{public}d", traceFile.c_str(), errno);
            g_tagsProperty = 0;
            return;
        }
    }
    g_tagsProperty = GetSysParamTags();
    std::string pidStr = std::to_string(getpid());
    errno_t ret = strcpy_s(g_pid, PID_BUF_SIZE, pidStr.c_str());
    if (ret != 0) {
        strcpy_s(g_pid, PID_BUF_SIZE, pidStr.c_str());
    }

    if (WatchParameter(KEY_TRACE_TAG.c_str(), ParameterChange, nullptr) != 0) {
        HiLog::Error(LABEL, "WatchParameter %{public}s failed", KEY_TRACE_TAG.c_str());
        return;
    }
    g_isHitraceMeterInit = true;
}
}; // namespace

void WriteToTraceMarker(const char* buf, const int count)
{
    if (UNEXPECTANTLY(count <= 0)) {
        return;
    }
    if (write(g_markerFd, buf, count) < 0) {
        HiLog::Error(LABEL, "write trace_marker failed, %{public}d", errno);
    }
}

void AddTraceMarkerLarge(const std::string& name, MarkerType& type, const int64_t& value)
{
    std::string record;
    record += g_markTypes[type];
    record += "|";
    record += g_pid;
    record += "|H:";
    std::string nameNew = name;
    if (name.size() > NAME_MAX_SIZE) {
        nameNew = name.substr(0, NAME_MAX_SIZE);
    }
    record += nameNew;
    record += " ";
    if (value != 0) {
        record += std::to_string(value);
    }
    WriteToTraceMarker(record.c_str(), record.size());
}

void AddHitraceMeterMarker(MarkerType type, uint64_t& tag, const std::string& name, const int64_t& value)
{
    if (UNEXPECTANTLY(g_isHitraceMeterDisabled)) {
        return;
    }
    if (UNEXPECTANTLY(!g_isHitraceMeterInit)) {
        struct timespec ts = { 0, 0 };
        if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1 || ts.tv_sec < 20) { // 20 : register after boot 20s
            return;
        }
        std::call_once(g_onceFlag, OpenTraceMarkerFile);
    }
    if (UNEXPECTANTLY(g_tagsProperty & tag) && g_markerFd != -1) {
        // record fomart: "type|pid|name value".
        char buf[NAME_NORMAL_LEN];
        int len = name.length();
        if (UNEXPECTANTLY(len <= NAME_NORMAL_LEN)) {
            int bytes = 0;
            if (type == MARKER_BEGIN) {
                bytes = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1,
                    "B|%s|H:%s ", g_pid, name.c_str());
            } else if (type == MARKER_END) {
                bytes = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1,
                    "E|%s|", g_pid);
            } else {
                std::string marktypestr = g_markTypes[type];
                bytes = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1,
                    "%s|%s|H:%s %lld", marktypestr.c_str(), g_pid, name.c_str(), value);
            }
            WriteToTraceMarker(buf, bytes);
        } else {
            AddTraceMarkerLarge(name, type, value);
        }
    }
}

void UpdateTraceLabel()
{
    if (!g_isHitraceMeterInit) {
        return;
    }
    g_tagsProperty = GetSysParamTags();
}

void SetTraceDisabled(bool disable)
{
    g_isHitraceMeterDisabled = disable;
}

void StartTrace(uint64_t label, const string& value, float limit UNUSED_PARAM)
{
    AddHitraceMeterMarker(MARKER_BEGIN, label, value, 0);
}

void StartTraceDebug(bool isDebug, uint64_t label, const string& value, float limit UNUSED_PARAM)
{
    if (!isDebug) {
        return;
    }
    AddHitraceMeterMarker(MARKER_BEGIN, label, value, 0);
}

void StartTraceArgs(uint64_t label, const char *fmt, ...)
{
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;
    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HiLog::Error(LABEL, "vsnprintf_s failed: %{public}d", errno);
        return;
    }
    AddHitraceMeterMarker(MARKER_BEGIN, label, name, 0);
}

void StartTraceArgsDebug(bool isDebug, uint64_t label, const char *fmt, ...)
{
    if (!isDebug) {
        return;
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HiLog::Error(LABEL, "vsnprintf_s failed: %{public}d", errno);
        return;
    }
    AddHitraceMeterMarker(MARKER_BEGIN, label, name, 0);
}

void FinishTrace(uint64_t label)
{
    AddHitraceMeterMarker(MARKER_END, label, EMPTY_TRACE_NAME, 0);
}

void FinishTraceDebug(bool isDebug, uint64_t label)
{
    if (!isDebug) {
        return;
    }
    AddHitraceMeterMarker(MARKER_END, label, EMPTY_TRACE_NAME, 0);
}

void StartAsyncTrace(uint64_t label, const string& value, int32_t taskId, float limit UNUSED_PARAM)
{
    AddHitraceMeterMarker(MARKER_ASYNC_BEGIN, label, value, taskId);
}

void StartAsyncTraceDebug(bool isDebug, uint64_t label, const string& value, int32_t taskId, float limit UNUSED_PARAM)
{
    if (!isDebug) {
        return;
    }
    AddHitraceMeterMarker(MARKER_ASYNC_BEGIN, label, value, taskId);
}

void StartAsyncTraceArgs(uint64_t label, int32_t taskId, const char *fmt, ...)
{
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HiLog::Error(LABEL, "vsnprintf_s failed: %{public}d", errno);
        return;
    }
    AddHitraceMeterMarker(MARKER_ASYNC_BEGIN, label, name, taskId);
}

void StartAsyncTraceArgsDebug(bool isDebug, uint64_t label, int32_t taskId, const char *fmt, ...)
{
    if (!isDebug) {
        return;
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HiLog::Error(LABEL, "vsnprintf_s failed: %{public}d", errno);
        return;
    }
    AddHitraceMeterMarker(MARKER_ASYNC_BEGIN, label, name, taskId);
}

void FinishAsyncTrace(uint64_t label, const string& value, int32_t taskId)
{
    AddHitraceMeterMarker(MARKER_ASYNC_END, label, value, taskId);
}

void FinishAsyncTraceDebug(bool isDebug, uint64_t label, const string& value, int32_t taskId)
{
    if (!isDebug) {
        return;
    }
    AddHitraceMeterMarker(MARKER_ASYNC_END, label, value, taskId);
}

void FinishAsyncTraceArgs(uint64_t label, int32_t taskId, const char *fmt, ...)
{
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HiLog::Error(LABEL, "vsnprintf_s failed: %{public}d", errno);
        return;
    }
    AddHitraceMeterMarker(MARKER_ASYNC_END, label, name, taskId);
}

void FinishAsyncTraceArgsDebug(bool isDebug, uint64_t label, int32_t taskId, const char *fmt, ...)
{
    if (!isDebug) {
        return;
    }
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HiLog::Error(LABEL, "vsnprintf_s failed: %{public}d", errno);
        return;
    }
    AddHitraceMeterMarker(MARKER_ASYNC_END, label, name, taskId);
}

void MiddleTrace(uint64_t label, const string& beforeValue UNUSED_PARAM, const std::string& afterValue)
{
    AddHitraceMeterMarker(MARKER_END, label, EMPTY_TRACE_NAME, 0);
    AddHitraceMeterMarker(MARKER_BEGIN, label, afterValue, 0);
}

void MiddleTraceDebug(bool isDebug, uint64_t label, const string& beforeValue UNUSED_PARAM, 
    const std::string& afterValue)
{
    if (!isDebug) {
        return;
    }
    AddHitraceMeterMarker(MARKER_END, label, EMPTY_TRACE_NAME, 0);
    AddHitraceMeterMarker(MARKER_BEGIN, label, afterValue, 0);
}

void CountTrace(uint64_t label, const string& name, int64_t count)
{
    AddHitraceMeterMarker(MARKER_INT, label, name, count);
}

void CountTraceDebug(bool isDebug, uint64_t label, const string& name, int64_t count)
{
    if (!isDebug) {
        return;
    }
    AddHitraceMeterMarker(MARKER_INT, label, name, count);
}

HitraceMeterFmtScoped::HitraceMeterFmtScoped(uint64_t label, const char *fmt, ...) : mTag(label)
{
    char name[VAR_NAME_MAX_SIZE] = { 0 };
    va_list args;

    va_start(args, fmt);
    int res = vsnprintf_s(name, sizeof(name), sizeof(name) - 1, fmt, args);
    va_end(args);
    if (res < 0) {
        HiLog::Error(LABEL, "vsnprintf_s failed: %{public}d", errno);
        return;
    }
    AddHitraceMeterMarker(MARKER_BEGIN, label, name, 0);
}
