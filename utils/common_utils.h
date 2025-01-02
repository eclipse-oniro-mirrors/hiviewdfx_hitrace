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

#ifndef HITRACE_COMMON_UTILS_H
#define HITRACE_COMMON_UTILS_H

#include <string>

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {
std::string CanonicalizeSpecPath(const char* src);

bool MarkClockSync(const std::string& traceRootPath);

bool IsNumber(const std::string &str);

int GetCpuProcessors();

void ReadCurrentCpuFrequencies(std::string& freqs);

std::string GetPropertyInner(const std::string& property, const std::string& value);

bool SetPropertyInner(const std::string& property, const std::string& value);

bool IsHmKernel();

bool IsDeveloperMode();

bool IsRootVersion();
} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS

#endif // HITRACE_COMMON_UTILS_H