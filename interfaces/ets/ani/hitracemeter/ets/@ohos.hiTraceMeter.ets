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

class HiTraceMeter {
    native static startTrace(name: string, taskId: number): void;
    native static finishTrace(name: string, taskId: number): void;
}

export default namespace hiTraceMeter {
    loadLibrary("hitrace_meter_ani");

    export function startTrace(name: string, taskId: number): void {
        HiTraceMeter.startTrace(name, taskId)
    }
    export function finishTrace(name: string, taskId: number): void {
        HiTraceMeter.finishTrace(name, taskId)
    }
}
