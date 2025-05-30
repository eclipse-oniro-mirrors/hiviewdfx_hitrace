/*
 * Copyright (C) 2021-2022 Huawei Device Co., Ltd.
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

import hiTraceChain from "@ohos.hiTraceChain"

import {describe, beforeAll, beforeEach, afterEach, afterAll, it, expect} from 'deccjsunit/index'

describe('hiTraceChainJsUnitTest', function () {
    let logTag = '[hiTraceChainJsUnitTest]';

    beforeAll(function() {

        /**
         * @tc.setup: setup invoked before all test cases
         */
        console.info(logTag, 'hiTraceChainJsUnitTest beforeAll called');
    })

    afterAll(function() {

        /**
         * @tc.teardown: teardown invoked after all test cases
         */
        console.info(logTag, 'hiTraceChainJsUnitTest afterAll called');
    })

    beforeEach(function() {

        /**
         * @tc.setup: setup invoked before each test case
         */
        console.info(logTag, 'hiTraceChainJsUnitTest beforeEach called');
    })

    afterEach(function() {

        /**
         * @tc.teardown: teardown invoked after each test case
         */
        console.info(logTag, 'hiTraceChainJsUnitTest afterEach called');
    })

    /**
     * @tc.name: hiTraceChainJsUnitTest001
     * @tc.desc: test hiTraceChain.begin/hiTraceChain.isValid/hiTraceChain.end
     * @tc.type: FUNC
     */
    it('hiTraceChainJsUnitTest001', 0, async function (done) {
        let traceId = hiTraceChain.begin("hiTraceChainJsUnitTest001");
        expect(hiTraceChain.isValid(traceId)).assertTrue();
        hiTraceChain.end(traceId);
        done();
    });

    /**
     * @tc.name: hiTraceChainJsUnitTest002
     * @tc.desc: test hiTraceChain.creadSpan
     * @tc.type: FUNC
     */
    it('hiTraceChainJsUnitTest002', 0, async function (done) {
        let traceId = hiTraceChain.begin("hiTraceChainJsUnitTest002");
        traceId = hiTraceChain.createSpan();
        expect(hiTraceChain.isValid(traceId)).assertTrue();
        hiTraceChain.end(traceId);
        done();
    });

    /**
     * @tc.name: hiTraceChainJsUnitTest003
     * @tc.desc: test hiTraceChain.tracepoint
     * @tc.type: FUNC
     */
     it('hiTraceChainJsUnitTest003', 0, async function (done) {
        let traceId = hiTraceChain.begin("hiTraceChainJsUnitTest003");
        expect(hiTraceChain.isValid(traceId)).assertTrue();
        hiTraceChain.tracepoint(hiTraceChain.HiTraceCommunicationMode.DEFAULT,
            hiTraceChain.HiTraceTracepointType.CS, traceId, "hiTraceChainJsUnitTest003 test case");
        expect(hiTraceChain.isValid(traceId)).assertTrue();
        hiTraceChain.end(traceId);
        done();
    });

    /**
     * @tc.name: hiTraceChainJsUnitTest004
     * @tc.desc: test hiTraceChain.isFlagEnabled
     * @tc.type: FUNC
     */
    it('hiTraceChainJsUnitTest004', 0, async function (done) {
        let traceId = hiTraceChain.begin("hiTraceChainJsUnitTest004")
        expect(!hiTraceChain.isFlagEnabled(traceId, hiTraceChain.HiTraceFlag.INCLUDE_ASYNC)).assertTrue();
        hiTraceChain.enableFlag(traceId, hiTraceChain.HiTraceFlag.INCLUDE_ASYNC);
        expect(hiTraceChain.isFlagEnabled(traceId, hiTraceChain.HiTraceFlag.INCLUDE_ASYNC)).assertTrue();
        hiTraceChain.end(traceId);
        done();
    });

    /**
     * @tc.name: hiTraceChainJsUnitTest005
     * @tc.desc: test call begin api function with parameters of wrong number
     * @tc.type: FUNC
     */
    it('hiTraceChainJsUnitTest005', 0, async function (done) {
        let traceId = hiTraceChain.begin("hiTraceChainJsUnitTest005", "testp1", "testp2");
        expect(!hiTraceChain.isValid(traceId)).assertTrue();
        hiTraceChain.end(traceId);
        done();
    });

    /**
     * @tc.name: hiTraceChainJsUnitTest006
     * @tc.desc: test hiTraceChain.begin with undefined flag
     * @tc.type: FUNC
     */
    it('hiTraceChainJsUnitTest006', 0, async function (done) {
        let traceId = hiTraceChain.begin("hiTraceChainJsUnitTest006", undefined);
        expect(hiTraceChain.isValid(traceId)).assertTrue();
        hiTraceChain.end(traceId);
        done();
    });

    /**
     * @tc.name: hiTraceChainJsUnitTest007
     * @tc.desc: test hiTraceChain.tracepoint with undefined msg
     * @tc.type: FUNC
     */
    it('hiTraceChainJsUnitTest007', 0, async function (done) {
        let traceId = hiTraceChain.begin("hiTraceChainJsUnitTest007");
        expect(hiTraceChain.isValid(traceId)).assertTrue();
        hiTraceChain.tracepoint(hiTraceChain.HiTraceCommunicationMode.DEFAULT,
            hiTraceChain.HiTraceTracepointType.CS, traceId, undefined);
        expect(hiTraceChain.isValid(traceId)).assertTrue();
        hiTraceChain.end(traceId);
        done();
    });

    /**
     * @tc.name: hiTraceChainJsUnitTest008
     * @tc.desc: test hiTraceChain.begin with null flag
     * @tc.type: FUNC
     */
    it('hiTraceChainJsUnitTest008', 0, async function (done) {
        let traceId = hiTraceChain.begin("hiTraceChainJsUnitTest008", null);
        expect(hiTraceChain.isValid(traceId)).assertTrue();
        hiTraceChain.end(traceId);
        done();
    });

    /**
     * @tc.name: hiTraceChainJsUnitTest009
     * @tc.desc: test hiTraceChain.tracepoint with null msg
     * @tc.type: FUNC
     */
    it('hiTraceChainJsUnitTest009', 0, async function (done) {
        let traceId = hiTraceChain.begin("hiTraceChainJsUnitTest009");
        expect(hiTraceChain.isValid(traceId)).assertTrue();
        hiTraceChain.tracepoint(hiTraceChain.HiTraceCommunicationMode.DEFAULT,
            hiTraceChain.HiTraceTracepointType.CS, traceId, null);
        expect(hiTraceChain.isValid(traceId)).assertTrue();
        hiTraceChain.end(traceId);
        done();
    });

    /**
     * @tc.name: hiTraceChainJsUnitTest010
     * @tc.desc: test hiTraceChain.tracepoint with no msg
     * @tc.type: FUNC
     */
    it('hiTraceChainJsUnitTest010', 0, async function (done) {
        let traceId = hiTraceChain.begin("hiTraceChainJsUnitTest010");
        expect(hiTraceChain.isValid(traceId)).assertTrue();
        hiTraceChain.tracepoint(hiTraceChain.HiTraceCommunicationMode.DEFAULT,
            hiTraceChain.HiTraceTracepointType.CS, traceId);
        expect(hiTraceChain.isValid(traceId)).assertTrue();
        hiTraceChain.end(traceId);
        done();
    });
});
