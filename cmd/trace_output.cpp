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

#include "trace_output.h"

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common_utils.h"
#include "trace_file_utils.h"

namespace OHOS::HiviewDFX::Hitrace {
bool PrepareTraceOutput(const std::string& outputPath, SmartFd& outFileFd, std::string& errorMessage)
{
    if (outputPath.size() > 0) {
        std::string outSpecPath = CanonicalizeSpecPath(outputPath.c_str());
        if (outSpecPath.empty() || !IsWritable(outSpecPath)) {
            errorMessage = "error: output path is illegal: " + outputPath;
            return false;
        }
        outFileFd = OHOS::HiviewDFX::SmartFd(
            open(outSpecPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
        if (!outFileFd) {
            errorMessage = "error: opening " + outputPath + ", errno: " +
                std::to_string(errno);
            return false;
        }
    }
    int outFd = outFileFd ? outFileFd.GetFd() : STDOUT_FILENO;
    struct stat outputStat {};
    if (fstat(outFd, &outputStat) != 0 ||
        (S_ISREG(outputStat.st_mode) && !SetTraceFileSource(outFd, TraceFileSource::USER))) {
        errorMessage = "error: setting trace output source failed, errno: " + std::to_string(errno);
        if (outFileFd) {
            outFileFd.Reset();
            (void)remove(outputPath.c_str());
        }
        return false;
    }
    return true;
}

} // namespace OHOS::HiviewDFX::Hitrace
