/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "hitraceoption_fuzzer.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <iostream>
#include <unistd.h>
#include <vector>

#include "hitrace_option/hitrace_option.h"
#include "hitrace_fuzztest_common.h"

namespace OHOS {
namespace HiviewDFX {
namespace Hitrace {

void FilterAppTest(const uint8_t* data, size_t size)
{
    pid_t pid = 0;
    if (size < sizeof(pid)) {
        return;
    }
    StreamToValueInfo(data, pid);
    std::string app(reinterpret_cast<const char*>(data), size - sizeof(pid));
    SetFilterAppName(app);
    AddFilterPid(pid);
    FilterAppTrace(app, pid);
    FilterAppTrace(app.c_str(), pid);
    ClearFilterPid();
}

} // namespace Hitrace
} // namespace HiviewDFX
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) {
        return 0;
    }

    /* Run your code on data */
    OHOS::HiviewDFX::Hitrace::FilterAppTest(data, size);
    return 0;
}
