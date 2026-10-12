/*
 * Copyright (C) 2022-2024 Huawei Device Co., Ltd.
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

#ifndef HITRACE_CMD_TRACE_OUTPUT_H
#define HITRACE_CMD_TRACE_OUTPUT_H

#include <string>

#include "smart_fd.h"

namespace OHOS::HiviewDFX::Hitrace {
bool PrepareTraceOutput(const std::string& outputPath, SmartFd& outFileFd, std::string& errorMessage);
} // namespace OHOS::HiviewDFX::Hitrace
#endif // HITRACE_CMD_TRACE_OUTPUT_H
