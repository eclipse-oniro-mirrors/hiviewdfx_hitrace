/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "hitrace/hitracechain.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <sys/time.h>

#include "gtest/gtest-message.h"
#include "gtest/gtest-test-part.h"
#include "gtest/gtest_pred_impl.h"
#include "gtest/hwext/gtest-tag.h"
#include "hitrace/hitracechainc.h"
#include "hitrace/hitraceid.h"
#include "hitrace_meter.h"
#include "hitrace_meter_c.h"

#define ARRAY_FIRST_INDEX 0
#define ARRAY_SECOND_INDEX 1
#define ARRAY_THIRD_INDEX 2
#define HASH_DATA_LENGTH 3

#define DEVICE_CLIENT_SEND 12
#define PROCESS_CLIENT_SEND 22
#define THREAD_CLIENT_SEND 32
#define DEFAULT_CLIENT_SEND 42

static uint64_t HashFunc(const void* pData, uint32_t dataLen)
{
    const uint64_t seed = 131;
    if ((!pData) || dataLen == 0) {
        return 0;
    }
    uint64_t hash = 0;
    uint64_t len = dataLen;
    const char* p = static_cast<const char*>(pData);
    for (; len > 0; --len) {
        hash = (hash * seed) + (*p++);
    }
    return hash;
}

static uint64_t GenerateChainId()
{
    const uint64_t randomNum = 269;
    uint64_t hashData[HASH_DATA_LENGTH];
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    srand(tv.tv_sec);
    hashData[ARRAY_FIRST_INDEX] = tv.tv_sec;
    hashData[ARRAY_SECOND_INDEX] = tv.tv_usec;
    hashData[ARRAY_THIRD_INDEX] = randomNum;
    uint64_t hash = HashFunc(hashData, HASH_DATA_LENGTH * sizeof(uint64_t));
    return hash;
}


static uint64_t GenerateSpanId()
{
    const uint64_t randomNum = 269;
    uint64_t hashData[HASH_DATA_LENGTH];
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    srand(tv.tv_sec);
    hashData[ARRAY_FIRST_INDEX] = randomNum;
    hashData[ARRAY_SECOND_INDEX] = tv.tv_sec;
    hashData[ARRAY_THIRD_INDEX] = tv.tv_usec;
    uint64_t hash = HashFunc(hashData, HASH_DATA_LENGTH * sizeof(uint64_t));
    return hash;
}

static uint64_t GenerateParentSpanId()
{
    const uint64_t randomNum = 269;
    uint64_t hashData[HASH_DATA_LENGTH];
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    srand(tv.tv_sec);
    hashData[ARRAY_FIRST_INDEX] = tv.tv_usec;
    hashData[ARRAY_SECOND_INDEX] = randomNum;
    hashData[ARRAY_THIRD_INDEX] = tv.tv_sec;
    uint64_t hash = HashFunc(hashData, HASH_DATA_LENGTH * sizeof(uint64_t));
    return hash;
}

namespace OHOS {
namespace HiviewDFX {
using namespace testing::ext;

class HiTraceChainCppTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void HiTraceChainCppTest::SetUpTestCase()
{}

void HiTraceChainCppTest::TearDownTestCase()
{}

void HiTraceChainCppTest::SetUp()
{
    HiTraceChain::ClearId();
}

void HiTraceChainCppTest::TearDown()
{}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_IdTest_001
 * @tc.desc: Get, set and clear trace id
 * @tc.type: FUNC
 * @tc.require: AR000CQV9U
 */
HWTEST_F(HiTraceChainCppTest, IdTest_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get and validate trace id.
     * @tc.expected: step1. trace id is invalid.
     * @tc.steps: step2. construct trace id with chain id, span id, parent span id
     *     and flags and set it into context, then get and validate it.
     * @tc.expected: step2. trace id is valid with same chain id, span id, parent
     *     span id and flags.
     * @tc.steps: step3. construct invalid trace id and set into context, then get
     *     and validate it.
     * @tc.expected: step3. trace id is the same with step2.
     * @tc.steps: step4. clear trace id, then get and validate it.
     * @tc.expected: step4. trace id is invalid.
     */
    HiTraceId initId = HiTraceChain::GetId();
    EXPECT_EQ(0, initId.IsValid());
    /* set thread id */
    constexpr uint64_t CHAIN_ID = 0xABCDEF;
    constexpr uint64_t SPAN_ID = 0x12345;
    constexpr uint64_t PARENT_SPAN_ID = 0x67890;

    initId.SetChainId(CHAIN_ID);
    initId.EnableFlag(HITRACE_FLAG_INCLUDE_ASYNC);
    initId.EnableFlag(HITRACE_FLAG_DONOT_CREATE_SPAN);
    initId.SetSpanId(SPAN_ID);
    initId.SetParentSpanId(PARENT_SPAN_ID);

    EXPECT_EQ(1, initId.IsValid());
    EXPECT_EQ(CHAIN_ID, initId.GetChainId());
    EXPECT_EQ(HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_DONOT_CREATE_SPAN, initId.GetFlags());
    EXPECT_EQ(SPAN_ID, initId.GetSpanId());
    EXPECT_EQ(PARENT_SPAN_ID, initId.GetParentSpanId());

    HiTraceChain::SetId(initId);

    HiTraceId getId = HiTraceChain::GetId();
    EXPECT_EQ(1, getId.IsValid());
    EXPECT_EQ(CHAIN_ID, getId.GetChainId());
    EXPECT_EQ(HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_DONOT_CREATE_SPAN, getId.GetFlags());
    EXPECT_EQ(SPAN_ID, getId.GetSpanId());
    EXPECT_EQ(PARENT_SPAN_ID, getId.GetParentSpanId());

    HiTraceId invalidId;
    HiTraceChain::SetId(invalidId);

    getId = HiTraceChain::GetId();
    EXPECT_EQ(1, getId.IsValid());
    EXPECT_EQ(CHAIN_ID, getId.GetChainId());
    EXPECT_EQ(HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_DONOT_CREATE_SPAN, getId.GetFlags());
    EXPECT_EQ(SPAN_ID, getId.GetSpanId());
    EXPECT_EQ(PARENT_SPAN_ID, getId.GetParentSpanId());

    HiTraceChain::ClearId();
    HiTraceId clearId = HiTraceChain::GetId();
    EXPECT_EQ(0, clearId.IsValid());
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_IdTest_002
 * @tc.desc: Get, set and clear trace id by trace id pointer
 * @tc.type: FUNC
 * @tc.require: AR000CQV9U
 */
HWTEST_F(HiTraceChainCppTest, IdTest_002, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get and validate trace id.
     * @tc.expected: step1. trace id is invalid.
     * @tc.steps: step2. construct trace id with chain id, span id, parent span id
     *     and flags and set it into context, then get and validate it.
     * @tc.expected: step2. trace id is valid with same chain id, span id, parent
     *     span id and flags.
     * @tc.steps: step3. construct invalid trace id and set into context, then get
     *     and validate it.
     * @tc.expected: step3. trace id is the same with step2.
     * @tc.steps: step4. clear trace id, then get and validate it.
     * @tc.expected: step4. trace id is invalid.
     */
    HiTraceId* pTraceId = HiTraceChain::GetIdAddress();
    EXPECT_EQ(0, pTraceId->IsValid());
    /* set thread id */
    constexpr uint64_t CHAIN_ID = 0xABCDEF;
    constexpr uint64_t SPAN_ID = 0x12345;
    constexpr uint64_t PARENT_SPAN_ID = 0x67890;

    HiTraceId initId = HiTraceChain::GetId();
    initId.SetChainId(CHAIN_ID);
    initId.EnableFlag(HITRACE_FLAG_INCLUDE_ASYNC);
    initId.EnableFlag(HITRACE_FLAG_DONOT_CREATE_SPAN);
    initId.SetSpanId(SPAN_ID);
    initId.SetParentSpanId(PARENT_SPAN_ID);

    HiTraceChain::SetId(initId);

    EXPECT_EQ(1, pTraceId->IsValid());
    EXPECT_EQ(CHAIN_ID, pTraceId->GetChainId());
    EXPECT_EQ(HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_DONOT_CREATE_SPAN, pTraceId->GetFlags());
    EXPECT_EQ(SPAN_ID, pTraceId->GetSpanId());
    EXPECT_EQ(PARENT_SPAN_ID, pTraceId->GetParentSpanId());

    HiTraceId invalidId;
    HiTraceChain::SetId(invalidId);

    EXPECT_EQ(1, pTraceId->IsValid());
    EXPECT_EQ(CHAIN_ID, pTraceId->GetChainId());
    EXPECT_EQ(HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_DONOT_CREATE_SPAN, pTraceId->GetFlags());
    EXPECT_EQ(SPAN_ID, pTraceId->GetSpanId());
    EXPECT_EQ(PARENT_SPAN_ID, pTraceId->GetParentSpanId());

    HiTraceChain::ClearId();
    EXPECT_EQ(0, pTraceId->IsValid());
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_IntfTest_001
 * @tc.desc: Interconversion between trace id and bytes array.
 * @tc.type: FUNC
 * @tc.require: AR000CQV9U
 */
HWTEST_F(HiTraceChainCppTest, IntfTest_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get trace id and validate it.
     * @tc.expected: step1. trace id is invalid.
     * @tc.steps: step2. construct trace id and validate it.
     * @tc.expected: step2. trace id is valid.
     * @tc.steps: step3. convert trace id to bytes array.
     * @tc.expected: step3. convert success when array size >= id length.
     * @tc.steps: step4. convert bytes array to trace id.
     * @tc.expected: step4. convert success only when array size == id length.
     * @tc.steps: step5. convert invalid id to bytes array.
     * @tc.expected: step5. convert fail.
     * @tc.steps: step6. convert invalid bytes array to id.
     * @tc.expected: step6. convert fail.
     */
    HiTraceId initId = HiTraceChain::GetId();
    EXPECT_EQ(0, initId.IsValid());
    constexpr uint64_t CHAIN_ID = 0xABCDEF;
    constexpr uint64_t SPAN_ID = 0x12345;
    constexpr uint64_t PARENT_SPAN_ID = 0x67890;
    constexpr int FLAGS = HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_DONOT_CREATE_SPAN;

    initId.SetChainId(CHAIN_ID);
    initId.SetFlags(FLAGS);
    initId.SetSpanId(SPAN_ID);
    initId.SetParentSpanId(PARENT_SPAN_ID);
    EXPECT_EQ(1, initId.IsValid());
    constexpr int ID_LEN = sizeof(HiTraceIdStruct);

    uint8_t bytes[ID_LEN + 1];
    int len = initId.ToBytes(bytes, ID_LEN - 1);
    EXPECT_EQ(0, len);
    len = initId.ToBytes(bytes, ID_LEN + 1);
    EXPECT_EQ(ID_LEN, len);
    len = initId.ToBytes(bytes, ID_LEN);
    EXPECT_EQ(ID_LEN, len);

    /* bytes to id */
    HiTraceId bytesToId = HiTraceId(bytes, ID_LEN - 1);
    EXPECT_EQ(0, bytesToId.IsValid());
    bytesToId = HiTraceId(bytes, ID_LEN + 1);
    EXPECT_EQ(0, bytesToId.IsValid());
    bytesToId = HiTraceId(bytes, ID_LEN);
    EXPECT_EQ(1, bytesToId.IsValid());
    EXPECT_EQ(CHAIN_ID, bytesToId.GetChainId());
    EXPECT_EQ(HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_DONOT_CREATE_SPAN, bytesToId.GetFlags());
    EXPECT_EQ(SPAN_ID, bytesToId.GetSpanId());
    EXPECT_EQ(PARENT_SPAN_ID, bytesToId.GetParentSpanId());

    /* set invalid id */
    HiTraceId invalidId;
    EXPECT_EQ(0, invalidId.ToBytes(bytes, ID_LEN));
    invalidId = HiTraceId(nullptr, ID_LEN);
    EXPECT_EQ(0, invalidId.IsValid());
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_IntfTest_002
 * @tc.desc: Start and stop trace.
 * @tc.type: FUNC
 * @tc.require: AR000CQV9U
 */
HWTEST_F(HiTraceChainCppTest, IntfTest_002, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with flags, get trace id and validit it.
     * @tc.expected: step1. trace id and flags is valid.
     * @tc.steps: step2. stop trace, get trace id and validit it.
     * @tc.expected: step2. trace id is invalid.
     */
    HiTraceId beginId = HiTraceChain::Begin("test", HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_NO_BE_INFO);
    EXPECT_EQ(1, beginId.IsValid());
    EXPECT_EQ(1, beginId.IsFlagEnabled(HITRACE_FLAG_INCLUDE_ASYNC));
    EXPECT_EQ(1, beginId.IsFlagEnabled(HITRACE_FLAG_NO_BE_INFO));

    HiTraceChain::End(beginId);

    HiTraceId endId = HiTraceChain::GetId();
    EXPECT_EQ(0, endId.IsValid());
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_IntfTest_003
 * @tc.desc: Start and stop trace with reentered.
 * @tc.type: FUNC
 * @tc.require: AR000CQV9U
 */
HWTEST_F(HiTraceChainCppTest, IntfTest_003, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace twice and get 2nd trace id.
     * @tc.expected: step1. 2nd trace is invalid.
     * @tc.steps: step2. get trace id and check.
     * @tc.expected: step2. trace id is valid and same with 1st id.
     * @tc.steps: step3. set chain id with wrong id and get trace id.
     * @tc.expected: step3. trace id is valid and same with 1st id.
     * @tc.steps: step4. stop trace twice and get trace id.
     * @tc.expected: step4. trace id is invalid.
     */
    /* begin */
    HiTraceId beginId = HiTraceChain::Begin("begin", HITRACE_FLAG_INCLUDE_ASYNC);
    /* reenter begin */
    HiTraceId reBeginId = HiTraceChain::Begin("reenter begin", HITRACE_FLAG_TP_INFO);
    EXPECT_EQ(0, reBeginId.IsValid());
    EXPECT_NE(reBeginId.GetChainId(), beginId.GetChainId());
    EXPECT_EQ(0, reBeginId.IsFlagEnabled(HITRACE_FLAG_INCLUDE_ASYNC));
    EXPECT_EQ(0, reBeginId.IsFlagEnabled(HITRACE_FLAG_TP_INFO));

    /* reenter end */
    HiTraceChain::End(reBeginId);

    HiTraceId endId = HiTraceChain::GetId();
    EXPECT_EQ(1, endId.IsValid());
    EXPECT_EQ(endId.GetChainId(), beginId.GetChainId());
    EXPECT_EQ(1, endId.IsFlagEnabled(HITRACE_FLAG_INCLUDE_ASYNC));
    EXPECT_EQ(0, endId.IsFlagEnabled(HITRACE_FLAG_TP_INFO));

    /* end with wrong chainId */
    HiTraceId wrongBeginId = beginId;
    wrongBeginId.SetChainId(beginId.GetChainId() + 1);
    HiTraceChain::End(wrongBeginId);

    HiTraceId wrongEndId = HiTraceChain::GetId();
    EXPECT_EQ(1, wrongEndId.IsValid());
    EXPECT_EQ(wrongEndId.GetChainId(), beginId.GetChainId());
    EXPECT_EQ(1, wrongEndId.IsFlagEnabled(HITRACE_FLAG_INCLUDE_ASYNC));

    /* end */
    HiTraceChain::End(beginId);

    HiTraceId reEndId = HiTraceChain::GetId();
    EXPECT_EQ(0, reEndId.IsValid());

    /* end with invalid thread id */
    HiTraceChain::End(beginId);

    HiTraceId endInvalidId = HiTraceChain::GetId();
    EXPECT_EQ(0, endInvalidId.IsValid());
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_SpanTest_001
 * @tc.desc: Create child and grand child span.
 * @tc.type: FUNC
 * @tc.require: AR000CQV9U
 */
HWTEST_F(HiTraceChainCppTest, SpanTest_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace without HITRACE_FLAG_DONOT_CREATE_SPAN,
     *     get and check flags.
     * @tc.expected: step1. flags is same with set and span id is 0.
     * @tc.steps: step2. create child id.
     * @tc.expected: step2. child id has same span id with parent.
     * @tc.steps: step3. set child id into context.
     * @tc.steps: step4. create grand child id.
     * @tc.expected: step4. grand child id has same span id with parent and child.
     */
    /* begin with span flag */
    HiTraceId id = HiTraceChain::Begin("test", 0);
    EXPECT_EQ(0, id.GetFlags());
    EXPECT_EQ(0UL, id.GetSpanId());
    EXPECT_EQ(0UL, id.GetParentSpanId());

    /* create child span */
    HiTraceId childId = HiTraceChain::CreateSpan();
    EXPECT_EQ(1, childId.IsValid());
    EXPECT_EQ(childId.GetFlags(), id.GetFlags());
    EXPECT_EQ(childId.GetChainId(), id.GetChainId());
    EXPECT_EQ(childId.GetParentSpanId(), id.GetSpanId());

    /* set child id to thread id */
    HiTraceChain::SetId(childId);

    /* continue to create child span */
    HiTraceId grandChildId = HiTraceChain::CreateSpan();
    EXPECT_EQ(1, grandChildId.IsValid());
    EXPECT_EQ(grandChildId.GetFlags(), id.GetFlags());
    EXPECT_EQ(grandChildId.GetChainId(), id.GetChainId());
    EXPECT_EQ(grandChildId.GetParentSpanId(), childId.GetSpanId());

    /* end */
    HiTraceChain::End(id);
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_SpanTest_002
 * @tc.desc: Start and stop trace with reentered.
 * @tc.type: FUNC
 * @tc.require: AR000CQV9U
 */
HWTEST_F(HiTraceChainCppTest, SpanTest_002, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with HITRACE_FLAG_DONOT_CREATE_SPAN,
     *     get and check flags.
     * @tc.expected: step1. HITRACE_FLAG_DONOT_CREATE_SPAN is enabled.
     * @tc.steps: step2. create child id.
     * @tc.expected: step2. child id is same with parent id.
     */
    /* begin with "donot create span" flag */
    HiTraceId id = HiTraceChain::Begin("test", HITRACE_FLAG_DONOT_CREATE_SPAN);
    EXPECT_EQ(1, id.IsFlagEnabled(HITRACE_FLAG_DONOT_CREATE_SPAN));

    /* create child span */
    HiTraceId childId = HiTraceChain::CreateSpan();
    EXPECT_EQ(1, childId.IsValid());
    EXPECT_EQ(childId.GetFlags(), id.GetFlags());
    EXPECT_EQ(childId.GetChainId(), id.GetChainId());
    EXPECT_EQ(childId.GetSpanId(), id.GetSpanId());
    EXPECT_EQ(childId.GetParentSpanId(), id.GetParentSpanId());

    /* end */
    HiTraceChain::End(id);
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_TracepointTest_001
 * @tc.desc: Start trace with HITRACE_FLAG_TP_INFO flag.
 * @tc.type: FUNC
 * @tc.require: AR000CQVA3
 */
HWTEST_F(HiTraceChainCppTest, TracepointTest_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with HITRACE_FLAG_TP_INFO,
     *     get and check flags.
     * @tc.expected: step1. HITRACE_FLAG_TP_INFO is enabled.
     * @tc.steps: step2. add trace point info with id and check logs.
     * @tc.expected: step2. trace point can be found in logs.
     * @tc.steps: step3. add trace point info with null and check logs.
     * @tc.expected: step3. trace point cannot be found in logs.
     */
    /* begin with tp flag */
    HiTraceId invalidId;
    HiTraceId id = HiTraceChain::Begin("test tp flag", HITRACE_FLAG_TP_INFO);
    EXPECT_EQ(1, id.IsFlagEnabled(HITRACE_FLAG_TP_INFO));
    HiTraceChain::Tracepoint(HITRACE_TP_CS, id, "client send msg content %d", 12);
    HiTraceChain::Tracepoint(HITRACE_TP_CS, invalidId, "client send msg content %d", 12);
    HiTraceChain::End(id);
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_TracepointTest_002
 * @tc.desc: Start trace without HITRACE_FLAG_TP_INFO flag.
 * @tc.type: FUNC
 * @tc.require: AR000CQVA3
 */
HWTEST_F(HiTraceChainCppTest, TracepointTest_002, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace without HITRACE_FLAG_TP_INFO flag.
     *     get and check flags.
     * @tc.expected: step1. HITRACE_FLAG_TP_INFO is not enabled.
     * @tc.steps: step2. add trace point info with id and check logs.
     * @tc.expected: step2. trace point cannot be found in logs.
     */
    /* begin with tp flag */
    HiTraceId id = HiTraceChain::Begin("test no tp flag", HITRACE_FLAG_INCLUDE_ASYNC);
    EXPECT_EQ(0, id.IsFlagEnabled(HITRACE_FLAG_TP_INFO));
    HiTraceChain::Tracepoint(HITRACE_TP_CS, id, "client send msg content %d", 12);

    HiTraceChain::End(id);
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_TracepointTest_003
 * @tc.desc: Start trace with HITRACE_FLAG_D2D_TP_INFO flag.
 * @tc.type: FUNC
 * @tc.require: AR000CQVA3
 */
HWTEST_F(HiTraceChainCppTest, TracepointTest_003, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with HITRACE_FLAG_D2D_TP_INFO,
     *     get and check flags.
     * @tc.expected: step1. HITRACE_FLAG_D2D_TP_INFO is enabled.
     * @tc.steps: step2. add D2D trace point info with id and check logs.
     * @tc.expected: step2. trace point can be found in logs.
     * @tc.steps: step2. add D2D trace point info with null and check logs.
     * @tc.expected: step2. trace point cannot be found in logs.
     * @tc.steps: step3. add trace point info with id and check logs.
     * @tc.expected: step3. trace point cannot be found in logs.
     */
    HiTraceId id = HiTraceChain::Begin("test D2D tp flag", HITRACE_FLAG_D2D_TP_INFO);
    EXPECT_EQ(1, id.IsFlagEnabled(HITRACE_FLAG_D2D_TP_INFO));
    HiTraceChain::Tracepoint(HITRACE_CM_DEVICE, HITRACE_TP_CS, id, "client send msg content %d", 12);
    HiTraceChain::Tracepoint(HITRACE_CM_PROCESS, HITRACE_TP_CS, id, "cannot be found %d", 22);
    HiTraceChain::Tracepoint(HITRACE_CM_THREAD, HITRACE_TP_CS, id, "cannot be found %d", 32);
    HiTraceChain::Tracepoint(HITRACE_CM_DEFAULT, HITRACE_TP_CS, id, "cannot be found %d", 42);

    HiTraceId invalidId;
    HiTraceChain::Tracepoint(HITRACE_CM_DEVICE, HITRACE_TP_CS, invalidId, "cannot be found %d", 13);

    HiTraceChain::Tracepoint(HITRACE_TP_CS, id, "cannot be found %d", 14);

    HiTraceChain::End(id);
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_TracepointTest_004
 * @tc.desc: Start trace without HITRACE_FLAG_D2D_TP_INFO flag.
 * @tc.type: FUNC
 * @tc.require: AR000CQVA3
 */
HWTEST_F(HiTraceChainCppTest, TracepointTest_004, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace without HITRACE_FLAG_D2D_TP_INFO flag.
     *     get and check flags.
     * @tc.expected: step1. HITRACE_FLAG_D2D_TP_INFO is not enabled.
     * @tc.steps: step2. add D2D trace point info with id and check logs.
     * @tc.expected: step2. trace point cannot be found in logs.
     */
    HiTraceId id = HiTraceChain::Begin("test no D2D tp flag", HITRACE_FLAG_INCLUDE_ASYNC);
    EXPECT_EQ(0, id.IsFlagEnabled(HITRACE_FLAG_D2D_TP_INFO));
    HiTraceChain::Tracepoint(HITRACE_CM_DEVICE, HITRACE_TP_CS, id, "cannot be found %d", 12);
    HiTraceChain::Tracepoint(HITRACE_CM_PROCESS, HITRACE_TP_CS, id, "cannot be found %d", 22);
    HiTraceChain::Tracepoint(HITRACE_CM_THREAD, HITRACE_TP_CS, id, "cannot be found %d", 32);
    HiTraceChain::Tracepoint(HITRACE_CM_DEFAULT, HITRACE_TP_CS, id, "cannot be found %d", 42);

    HiTraceChain::End(id);
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_TracepointTest_005
 * @tc.desc: Start trace with HITRACE_FLAG_D2D_TP_INFO and HITRACE_FLAG_TP_INFO flag.
 * @tc.type: FUNC
 * @tc.require: AR000CQVA3
 */
HWTEST_F(HiTraceChainCppTest, TracepointTest_005, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with HITRACE_FLAG_D2D_TP_INFO | HITRACE_FLAG_TP_INFO,
     *     get and check flags.
     * @tc.expected: step1. HITRACE_FLAG_D2D_TP_INFO is enabled.
     * @tc.expected: step1. HITRACE_FLAG_TP_INFO is enabled.
     * @tc.steps: step2. add D2D trace point info with id and check logs.
     * @tc.expected: step2. trace point can be found in logs.
     * @tc.steps: step3. add trace point info with id and check logs.
     * @tc.expected: step3. trace point can be found in logs.
     */
    HiTraceId id = HiTraceChain::Begin("test D2D | TP tp flag", HITRACE_FLAG_D2D_TP_INFO | HITRACE_FLAG_TP_INFO);
    EXPECT_EQ(1, id.IsFlagEnabled(HITRACE_FLAG_D2D_TP_INFO));
    EXPECT_EQ(1, id.IsFlagEnabled(HITRACE_FLAG_TP_INFO));
    HiTraceChain::Tracepoint(HITRACE_CM_DEVICE, HITRACE_TP_CS, id, "client send msg content %d", 12);
    HiTraceChain::Tracepoint(HITRACE_CM_PROCESS, HITRACE_TP_CS, id, "client send msg content %d", 22);
    HiTraceChain::Tracepoint(HITRACE_CM_THREAD, HITRACE_TP_CS, id, "client send msg content %d", 32);
    HiTraceChain::Tracepoint(HITRACE_CM_DEFAULT, HITRACE_TP_CS, id, "client send msg content %d", 42);

    HiTraceChain::Tracepoint(HITRACE_TP_CS, id, "client send msg content %d", 13);

    HiTraceChain::End(id);
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_TracepointTest_006
 * @tc.desc: Start trace without HITRACE_FLAG_D2D_TP_INFO, but with HITRACE_FLAG_TP_INFO flag.
 * @tc.type: FUNC
 * @tc.require: AR000CQVA3
 */
HWTEST_F(HiTraceChainCppTest, TracepointTest_006, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with HITRACE_FLAG_TP_INFO flag.
     *     get and check flags.
     * @tc.expected: step1. HITRACE_FLAG_D2D_TP_INFO is not enabled.
     * * @tc.expected: step1. HITRACE_FLAG_TP_INFO is enabled.
     * @tc.steps: step2. add D2D trace point info with id and check logs.
     * @tc.expected: step2. trace point can be found in logs.
     * @tc.steps: step2. add trace point info with id and check logs.
     * @tc.expected: step2. trace point can be found in logs.
     */
    HiTraceId id = HiTraceChain::Begin("test no D2D, but tp flag", HITRACE_FLAG_TP_INFO);
    EXPECT_EQ(0, id.IsFlagEnabled(HITRACE_FLAG_D2D_TP_INFO));
    EXPECT_EQ(1, id.IsFlagEnabled(HITRACE_FLAG_TP_INFO));
    HiTraceChain::Tracepoint(HITRACE_CM_DEVICE, HITRACE_TP_CS, id, "client send msg content %d", 12);
    HiTraceChain::Tracepoint(HITRACE_CM_PROCESS, HITRACE_TP_CS, id, "client send msg content %d", 22);
    HiTraceChain::Tracepoint(HITRACE_CM_THREAD, HITRACE_TP_CS, id, "client send msg content %d", 32);
    HiTraceChain::Tracepoint(HITRACE_CM_DEFAULT, HITRACE_TP_CS, id, "client send msg content %d", 42);

    HiTraceChain::Tracepoint(HITRACE_TP_CS, id, "client send msg content %d", 13);

    HiTraceChain::End(id);
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_TracepointTest_007
 * @tc.desc: Start trace without HITRACE_FLAG_D2D_TP_INFO, but with HITRACE_FLAG_TP_INFO flag.
 * @tc.type: FUNC
 * @tc.require: AR000CQVA3
 */
HWTEST_F(HiTraceChainCppTest, TracepointTest_007, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with HITRACE_FLAG_TP_INFO flag.
     *     get and check flags.
     * @tc.expected: step1. HITRACE_FLAG_D2D_TP_INFO is not enabled.
     * * @tc.expected: step1. HITRACE_FLAG_TP_INFO is enabled.
     * @tc.steps: step2. add D2D trace point info with id and check logs.
     * @tc.expected: step2. trace point can be found in logs.
     * @tc.steps: step2. add trace point info with id and check logs.
     * @tc.expected: step2. trace point can be found in logs.
     */
    HiTraceId id = HiTraceChain::Begin("test no D2D, but tp flag", HITRACE_FLAG_TP_INFO);
    EXPECT_EQ(0, id.IsFlagEnabled(HITRACE_FLAG_D2D_TP_INFO));
    EXPECT_EQ(1, id.IsFlagEnabled(HITRACE_FLAG_TP_INFO));
    HiTraceChain::Tracepoint(HITRACE_TP_CS, id, "client send msg content %d", 12);
    HiTraceChain::Tracepoint(HITRACE_TP_CS, id, "client send msg content %d", 22);
    HiTraceChain::Tracepoint(HITRACE_TP_CS, id, "client send msg content %d", 32);
    HiTraceChain::Tracepoint(HITRACE_TP_CS, id, "client send msg content %d", 42);

    HiTraceChain::Tracepoint(HITRACE_TP_CS, id, "client send msg content %d", 13);

    HiTraceChain::End(id);
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_SyncAsyncTest_001
 * @tc.desc: Start trace with SYNC or ASYNC.
 * @tc.type: FUNC
 * @tc.require: AR000CQ0G7
 */
HWTEST_F(HiTraceChainCppTest, SyncAsyncTest_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace without HITRACE_FLAG_INCLUDE_ASYNC flag.
     *    get and check flags.
     * @tc.expected: step1. HITRACE_FLAG_INCLUDE_ASYNC is not enabled.
     * @tc.steps: step2. start trace with HITRACE_FLAG_INCLUDE_ASYNC flag.
     *    get and check flags.
     * @tc.expected: step2. HITRACE_FLAG_INCLUDE_ASYNC is enabled.
     */
    /* begin with sync flag */
    HiTraceId syncId = HiTraceChain::Begin("test sync only", HITRACE_FLAG_TP_INFO);
    EXPECT_EQ(0, syncId.IsFlagEnabled(HITRACE_FLAG_INCLUDE_ASYNC));
    HiTraceChain::Tracepoint(HITRACE_TP_CS, syncId, "client send msg: %s", "sync");
        HiTraceChain::End(syncId);
    /* begin with async flag */
    HiTraceId asyncId = HiTraceChain::Begin("test sync+async", HITRACE_FLAG_INCLUDE_ASYNC | HITRACE_FLAG_TP_INFO);
    EXPECT_EQ(1, asyncId.IsFlagEnabled(HITRACE_FLAG_INCLUDE_ASYNC));
    HiTraceChain::Tracepoint(HITRACE_TP_CS, asyncId, "client send msg: %s", "async");

    HiTraceChain::End(asyncId);
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_InvalidParamTest_001
 * @tc.desc: Start trace with SYNC or ASYNC.
 * @tc.type: FUNC
 * @tc.require: AR000CQV9U
 */
HWTEST_F(HiTraceChainCppTest, InvalidParamTest_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with invalid flag and validate trace id.
     * @tc.expected: step1. trace id is invalid.
     * @tc.steps: step2. start trace with invalid name and validate trace id.
     * @tc.expected: step2. trace id is valid.
     */
    /* begin with invalid flag */
    HiTraceId invalidFlagId = HiTraceChain::Begin("invalid param", HITRACE_FLAG_MAX+1);
    EXPECT_EQ(0, invalidFlagId.IsValid());
    invalidFlagId = HiTraceChain::Begin("invalid param", -1);
    EXPECT_EQ(0, invalidFlagId.IsValid());
    HiTraceChain::End(invalidFlagId);

    /* begin with invalid name */
    HiTraceId invalidNameId = HiTraceChain::Begin("", HITRACE_FLAG_TP_INFO);
    EXPECT_EQ(1, invalidNameId.IsValid());
    HiTraceChain::End(invalidNameId);
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_HiTraceTest_001
 * @tc.desc: Start trace with SYNC or ASYNC.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceChainCppTest, HiTraceTest_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with invalid flag and validate trace id.
     * @tc.expected: step1. trace id is invalid.
     * @tc.steps: step2. start trace with invalid name and validate trace id.
     * @tc.expected: step2. trace id is valid.
     */
    /* begin with invalid flag */
    HiTraceId invalidFlagId = HiTraceChain::Begin("invalid param", HITRACE_FLAG_MAX+1);
    EXPECT_EQ(0, invalidFlagId.IsValid());
    invalidFlagId = HiTraceChain::Begin("invalid param", -1);
    EXPECT_EQ(0, invalidFlagId.IsValid());
    HiTraceChain::End(invalidFlagId);

    /* begin with invalid name */
    HiTraceId invalidNameId = HiTraceChain::Begin("", HITRACE_FLAG_TP_INFO);
    EXPECT_EQ(1, invalidNameId.IsValid());
    HiTraceChain::End(invalidNameId);

    /* Function interface overlay */
    int taskId = 123;
    int count = 1;
    HiTraceStartTrace(HITRACE_TAG_OHOS, "HiTraceTest001");
    HiTraceFinishTrace(HITRACE_TAG_OHOS);
    HiTraceStartAsyncTrace(HITRACE_TAG_OHOS, "HiTraceTest001", taskId);
    HiTraceFinishAsyncTrace(HITRACE_TAG_OHOS, "HiTraceTest001", taskId);
    HiTraceCountTrace(HITRACE_TAG_OHOS, "HiTraceTest001", count);
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_HiTraceTest_002
 * @tc.desc: Create child and grand child span.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceChainCppTest, HiTraceTest_002, TestSize.Level1)
{
    /* begin with span flag */
    HiTraceId id = HiTraceChain::Begin("test", 0);
    EXPECT_EQ(0, id.GetFlags());
    EXPECT_EQ(0UL, id.GetSpanId());
    EXPECT_EQ(0UL, id.GetParentSpanId());

    /* create child span */
    HiTraceId childId = HiTraceChain::CreateSpan();
    EXPECT_EQ(1, childId.IsValid());
    EXPECT_EQ(childId.GetFlags(), id.GetFlags());
    EXPECT_EQ(childId.GetChainId(), id.GetChainId());
    EXPECT_EQ(childId.GetParentSpanId(), id.GetSpanId());

    /* set child id to thread id */
    HiTraceChain::SetId(childId);

    /* Restore child*/
    HiTraceChain::Restore(childId);

    /* save child and set child id to thread id */
    HiTraceChain::SaveAndSet(childId);

    /* continue to create child span */
    HiTraceId grandChildId = HiTraceChain::CreateSpan();
    EXPECT_EQ(1, grandChildId.IsValid());
    EXPECT_EQ(grandChildId.GetFlags(), id.GetFlags());
    EXPECT_EQ(grandChildId.GetChainId(), id.GetChainId());
    EXPECT_EQ(grandChildId.GetParentSpanId(), childId.GetSpanId());

    /* end */
    HiTraceChain::End(id);
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_RestoreTest_001
 * @tc.desc: Start normal trace.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceChainCppTest, RestoreTest_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace without any flag.
     * @tc.expected: step1. no flag is enabled.
     * @tc.steps: step2. generate a temporary trace id.
     * @tc.expected: step2. a trace id is generated.
     * @tc.steps: step3. set new trace id and save old trace id.
     * @tc.expected: step3. new trace id get into TLS.
     * @tc.steps: step4. store old trace id.
     * @tc.expected: step4. old trace id get into TLS.
     * @tc.steps: step5. end trace.
     * @tc.expected: step5. trace terminate.
     */

    // begin trace
    HiTraceId id = HiTraceChain::Begin("RestoreTest_001", HITRACE_FLAG_TP_INFO);

    // generate new trace id
    HiTraceIdStruct tempId{0};
    HiTraceChainInitId(&tempId);
    tempId.valid=HITRACE_ID_VALID;
    HiTraceId newId(tempId);
    uint64_t chainId = GenerateChainId();
    uint64_t spanId = GenerateSpanId();
    uint64_t parentSpanId = GenerateParentSpanId();
    newId.SetChainId(chainId);
    newId.SetSpanId(spanId);
    newId.SetParentSpanId(parentSpanId);

    // set new id and save old id
    HiTraceId oldId = HiTraceChain::SaveAndSet(newId);
    HiTraceId currentId = HiTraceChain::GetId();
    HiTraceChain::Tracepoint(
            HITRACE_CM_DEVICE, HITRACE_TP_CS, currentId, "client send msg %d", DEVICE_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_PROCESS, HITRACE_TP_CS, currentId, "client send msg %d", PROCESS_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_THREAD, HITRACE_TP_CS, currentId, "client send msg %d", THREAD_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_DEFAULT, HITRACE_TP_CS, currentId, "client send msg %d", DEFAULT_CLIENT_SEND);

    // restore old id
    HiTraceChain::Restore(oldId);
    HiTraceId currentId2 = HiTraceChain::GetId();
    EXPECT_EQ(id.GetChainId(), currentId2.GetChainId());
    EXPECT_EQ(id.GetSpanId(), currentId2.GetSpanId());
    EXPECT_EQ(id.GetParentSpanId(), currentId2.GetParentSpanId());
    HiTraceChain::Tracepoint(
            HITRACE_CM_DEVICE, HITRACE_TP_CS, currentId2, "client send msg %d", DEVICE_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_PROCESS, HITRACE_TP_CS, currentId2, "client send msg %d", PROCESS_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_THREAD, HITRACE_TP_CS, currentId2, "client send msg %d", THREAD_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_DEFAULT, HITRACE_TP_CS, currentId2, "client send msg %d", DEFAULT_CLIENT_SEND);

    // end trace
    HiTraceChain::End(id);
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_RestoreTest_002
 * @tc.desc: Start normal with HITRACE_FLAG_INCLUDE_ASYNC flag.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceChainCppTest, RestoreTest_002, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace with flag HITRACE_FLAG_INCLUDE_ASYNC.
     * @tc.expected: step1. HITRACE_FLAG_INCLUDE_ASYNC flag is enabled.
     * @tc.steps: step2. generate a temporary trace id.
     * @tc.expected: step2. a trace id is generated.
     * @tc.steps: step3. set new trace id and save old trace id.
     * @tc.expected: step3. new trace id get into TLS.
     * @tc.steps: step4. store old trace id.
     * @tc.expected: step4. old trace id get into TLS.
     * @tc.steps: step5. end trace.
     * @tc.expected: step5. trace terminate.
     */

    // begin trace
    HiTraceId id = HiTraceChain::Begin("RestoreTest_002", HITRACE_FLAG_TP_INFO | HITRACE_FLAG_INCLUDE_ASYNC);

    // generate new trace id
    HiTraceIdStruct tempId{0};
    HiTraceChainInitId(&tempId);
    tempId.valid=HITRACE_ID_VALID;
    HiTraceId newId(tempId);
    uint64_t chainId = GenerateChainId();
    uint64_t spanId = GenerateSpanId();
    uint64_t parentSpanId = GenerateParentSpanId();
    newId.SetChainId(chainId);
    newId.SetSpanId(spanId);
    newId.SetParentSpanId(parentSpanId);

    // set new id and save old id
    HiTraceId oldId = HiTraceChain::SaveAndSet(newId);
    HiTraceId currentId = HiTraceChain::GetId();
    HiTraceChain::Tracepoint(
            HITRACE_CM_DEVICE, HITRACE_TP_CS, currentId, "client send msg %d", DEVICE_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_PROCESS, HITRACE_TP_CS, currentId, "client send msg %d", PROCESS_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_THREAD, HITRACE_TP_CS, currentId, "client send msg %d", THREAD_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_DEFAULT, HITRACE_TP_CS, currentId, "client send msg %d", DEFAULT_CLIENT_SEND);

    // restore old id
    HiTraceChain::Restore(oldId);
    HiTraceId currentId2 = HiTraceChain::GetId();
    ASSERT_TRUE(currentId2.IsFlagEnabled(HITRACE_FLAG_INCLUDE_ASYNC));
    HiTraceChain::Tracepoint(
            HITRACE_CM_DEVICE, HITRACE_TP_CS, currentId2, "client send msg %d", DEVICE_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_PROCESS, HITRACE_TP_CS, currentId2, "client send msg %d", PROCESS_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_THREAD, HITRACE_TP_CS, currentId2, "client send msg %d", THREAD_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_DEFAULT, HITRACE_TP_CS, currentId2, "client send msg %d", DEFAULT_CLIENT_SEND);

    // end trace
    HiTraceChain::End(id);
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_RestoreTest_003
 * @tc.desc: Start normal trace and create span.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceChainCppTest, RestoreTest_003, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace without any flag, then create span.
     * @tc.expected: step1. no flag is enabled.
     * @tc.steps: step2. generate a temporary trace id.
     * @tc.expected: step2. a trace id is generated.
     * @tc.steps: step3. set new trace id and save old trace id.
     * @tc.expected: step3. new trace id get into TLS.
     * @tc.steps: step4. store old trace id.
     * @tc.expected: step4. old trace id get into TLS.
     * @tc.steps: step5. end trace.
     * @tc.expected: step5. trace terminate.
     */

    // begin trace
    HiTraceId id = HiTraceChain::Begin("RestoreTest_003", HITRACE_FLAG_TP_INFO);
    HiTraceChain::CreateSpan();

    // generate new trace id
    HiTraceIdStruct tempId{0};
    HiTraceChainInitId(&tempId);
    tempId.valid=HITRACE_ID_VALID;
    HiTraceId newId(tempId);
    uint64_t chainId = GenerateChainId();
    uint64_t spanId = GenerateSpanId();
    uint64_t parentSpanId = GenerateParentSpanId();
    newId.SetChainId(chainId);
    newId.SetSpanId(spanId);
    newId.SetParentSpanId(parentSpanId);

    // set new id and save old id
    HiTraceId oldId = HiTraceChain::SaveAndSet(newId);
    HiTraceChain::CreateSpan();
    HiTraceId currentId = HiTraceChain::GetId();
    HiTraceChain::Tracepoint(
            HITRACE_CM_DEVICE, HITRACE_TP_CS, currentId, "client send msg %d", DEVICE_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_PROCESS, HITRACE_TP_CS, currentId, "client send msg %d", PROCESS_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_THREAD, HITRACE_TP_CS, currentId, "client send msg %d", THREAD_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_DEFAULT, HITRACE_TP_CS, currentId, "client send msg %d", DEFAULT_CLIENT_SEND);

    // restore old id
    HiTraceChain::Restore(oldId);
    HiTraceId currentId2 = HiTraceChain::GetId();
    EXPECT_EQ(id.GetChainId(), currentId2.GetChainId());
    EXPECT_EQ(id.GetSpanId(), currentId2.GetSpanId());
    EXPECT_EQ(id.GetParentSpanId(), currentId2.GetParentSpanId());
    HiTraceChain::Tracepoint(
            HITRACE_CM_DEVICE, HITRACE_TP_CS, currentId2, "client send msg %d", DEVICE_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_PROCESS, HITRACE_TP_CS, currentId2, "client send msg %d", PROCESS_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_THREAD, HITRACE_TP_CS, currentId2, "client send msg %d", THREAD_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_DEFAULT, HITRACE_TP_CS, currentId2, "client send msg %d", DEFAULT_CLIENT_SEND);

    // end trace
    HiTraceChain::End(id);
}

/**
 * @tc.name: Dfx_HiTraceChainCppTest_RestoreTest_004
 * @tc.desc: Start normal trace.
 * @tc.type: FUNC
 */
HWTEST_F(HiTraceChainCppTest, RestoreTest_004, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start trace without any flag.
     * @tc.expected: step1. no flag is enabled.
     * @tc.steps: step2. generate a temporary trace id with HITRACE_ID_INVALID flag.
     * @tc.expected: step2. a trace id is generated.
     * @tc.steps: step3. set new trace id and save old trace id.
     * @tc.expected: step3. new trace id get into TLS.
     * @tc.steps: step4. store old trace id.
     * @tc.expected: step4. old trace id get into TLS.
     * @tc.steps: step5. end trace.
     * @tc.expected: step5. trace terminate.
     */

    // begin trace
    HiTraceId id = HiTraceChain::Begin("RestoreTest_004", HITRACE_FLAG_TP_INFO);

    // generate new trace id
    HiTraceIdStruct tempId{0};
    HiTraceChainInitId(&tempId);
    tempId.valid=HITRACE_ID_INVALID;
    HiTraceId newId(tempId);
    uint64_t chainId = GenerateChainId();
    uint64_t spanId = GenerateSpanId();
    uint64_t parentSpanId = GenerateParentSpanId();
    newId.SetChainId(chainId);
    newId.SetSpanId(spanId);
    newId.SetParentSpanId(parentSpanId);

    // set new id and save old id
    HiTraceId oldId = HiTraceChain::SaveAndSet(newId);
    HiTraceId currentId = HiTraceChain::GetId();
    HiTraceChain::Tracepoint(
            HITRACE_CM_DEVICE, HITRACE_TP_CS, currentId, "client send msg %d", DEVICE_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_PROCESS, HITRACE_TP_CS, currentId, "client send msg %d", PROCESS_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_THREAD, HITRACE_TP_CS, currentId, "client send msg %d", THREAD_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_DEFAULT, HITRACE_TP_CS, currentId, "client send msg %d", DEFAULT_CLIENT_SEND);

    // restore old id
    HiTraceChain::Restore(oldId);
    HiTraceId currentId2 = HiTraceChain::GetId();
    EXPECT_EQ(id.GetChainId(), currentId2.GetChainId());
    EXPECT_EQ(id.GetSpanId(), currentId2.GetSpanId());
    EXPECT_EQ(id.GetParentSpanId(), currentId2.GetParentSpanId());
    HiTraceChain::Tracepoint(
            HITRACE_CM_DEVICE, HITRACE_TP_CS, currentId2, "client send msg %d", DEVICE_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_PROCESS, HITRACE_TP_CS, currentId2, "client send msg %d", PROCESS_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_THREAD, HITRACE_TP_CS, currentId2, "client send msg %d", THREAD_CLIENT_SEND);
    HiTraceChain::Tracepoint(
            HITRACE_CM_DEFAULT, HITRACE_TP_CS, currentId2, "client send msg %d", DEFAULT_CLIENT_SEND);

    // end trace
    HiTraceChain::End(id);
}
}  // namespace HiviewDFX
}  // namespace OHOS
