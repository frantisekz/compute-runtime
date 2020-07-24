/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/mock_sysman_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_sysfs_frequency.h"

#include <cmath>

using ::testing::Invoke;

namespace L0 {
namespace ult {

constexpr double minFreq = 300.0;
constexpr double maxFreq = 1100.0;
constexpr double step = 100.0 / 6;
constexpr double request = 300.0;
constexpr double tdp = 1100.0;
constexpr double actual = 300.0;
constexpr double efficient = 300.0;
constexpr double maxVal = 1100.0;
constexpr double minVal = 300.0;
constexpr uint32_t numClocks = static_cast<uint32_t>((maxFreq - minFreq) / step) + 1;
constexpr uint32_t handleComponentCount = 1u;
class SysmanDeviceFrequencyFixture : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<Mock<FrequencySysfsAccess>> pSysfsAccess;
    SysfsAccess *pSysfsAccessOld = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<NiceMock<Mock<FrequencySysfsAccess>>>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        pSysfsAccess->setVal(minFreqFile, minFreq);
        pSysfsAccess->setVal(maxFreqFile, maxFreq);
        pSysfsAccess->setVal(requestFreqFile, request);
        pSysfsAccess->setVal(tdpFreqFile, tdp);
        pSysfsAccess->setVal(actualFreqFile, actual);
        pSysfsAccess->setVal(efficientFreqFile, efficient);
        pSysfsAccess->setVal(maxValFreqFile, maxVal);
        pSysfsAccess->setVal(minValFreqFile, minVal);
        ON_CALL(*pSysfsAccess.get(), read(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getVal));
        ON_CALL(*pSysfsAccess.get(), write(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::setVal));

        // delete handles created in initial SysmanDeviceHandleContext::init() call
        for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handle_list) {
            delete handle;
        }
        pSysmanDeviceImp->pFrequencyHandleContext->handle_list.clear();
        pSysmanDeviceImp->pFrequencyHandleContext->init();
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
    }

    double clockValue(const double calculatedClock) {
        // i915 specific. frequency step is a fraction
        // However, the i915 represents all clock
        // rates as integer values. So clocks are
        // rounded to the nearest integer.
        uint32_t actualClock = static_cast<uint32_t>(calculatedClock + 0.5);
        return static_cast<double>(actualClock);
    }

    std::vector<zes_freq_handle_t> get_freq_handles(uint32_t count) {
        std::vector<zes_freq_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumFrequencyDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceFrequencyFixture, GivenComponentCountZeroWhenEnumeratingFrequencyHandlesThenNonZeroCountIsReturnedAndCallSucceds) {
    uint32_t count;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &count, nullptr));
    EXPECT_EQ(count, handleComponentCount);

    uint32_t testCount = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &testCount, nullptr));
    EXPECT_EQ(count, testCount);

    auto handles = get_freq_handles(count);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenActualComponentCountTwoWhenTryingToGetOneComponentOnlyThenOneComponentIsReturnedAndCountUpdated) {
    auto pFrequencyHandleContextTest = std::make_unique<FrequencyHandleContext>(pOsSysman);
    pFrequencyHandleContextTest->handle_list.push_back(new FrequencyImp(pOsSysman));
    pFrequencyHandleContextTest->handle_list.push_back(new FrequencyImp(pOsSysman));

    uint32_t count = 1;
    std::vector<zes_freq_handle_t> phFrequency(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyHandleContextTest->frequencyGet(&count, phFrequency.data()));
    EXPECT_EQ(count, 1u);
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetPropertiesThenSuccessIsReturned) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_freq_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));
        EXPECT_EQ(ZES_STRUCTURE_TYPE_FREQ_PROPERTIES, properties.stype);
        EXPECT_EQ(nullptr, properties.pNext);
        EXPECT_EQ(ZES_FREQ_DOMAIN_GPU, properties.type);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_DOUBLE_EQ(maxFreq, properties.max);
        EXPECT_DOUBLE_EQ(minFreq, properties.min);
        EXPECT_TRUE(properties.canControl);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndZeroCountWhenCallingzesFrequencyGetAvailableClocksThenCallSucceeds) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
        EXPECT_EQ(numClocks, count);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndCorrectCountWhenCallingzesFrequencyGetAvailableClocksThenCallSucceeds) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        uint32_t count = 0;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
        EXPECT_EQ(numClocks, count);

        double *clocks = new double[count];
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, clocks));
        EXPECT_EQ(numClocks, count);
        for (uint32_t i = 0; i < count; i++) {
            EXPECT_DOUBLE_EQ(clockValue(minFreq + (step * i)), clocks[i]);
        }
        delete[] clocks;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidateFrequencyGetRangeWhengetMaxFailsThenFrequencyGetRangeCallShouldFail) {
    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValReturnError));
    auto pFrequencyImp = std::make_unique<FrequencyImp>(pOsSysman);
    zes_freq_range_t limit = {};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFrequencyImp->frequencyGetRange(&limit));
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetRangeThenVerifyzesFrequencyGetRangeTestCallSucceeds) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        zes_freq_range_t limits;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(minFreq, limits.min);
        EXPECT_DOUBLE_EQ(maxFreq, limits.max);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyLimitsWhenCallingFrequencySetRangeForFailures1ThenAPIExitsGracefully) {
    auto pFrequencyImp = std::make_unique<FrequencyImp>(pOsSysman);
    zes_freq_range_t limits = {};

    // Verify that Max must be within range.
    limits.min = minFreq;
    limits.max = 600.0;
    ON_CALL(*pSysfsAccess.get(), write(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::setValMinReturnError));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFrequencyImp->frequencySetRange(&limits));
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyLimitsWhenCallingFrequencySetRangeForFailures2ThenAPIExitsGracefully) {
    auto pFrequencyImp = std::make_unique<FrequencyImp>(pOsSysman);
    zes_freq_range_t limits = {};

    // Verify that Max must be within range.
    limits.min = 900.0;
    limits.max = maxFreq;
    ON_CALL(*pSysfsAccess.get(), write(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::setValMaxReturnError));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFrequencyImp->frequencySetRange(&limits));
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeThenVerifyzesFrequencySetRangeTest1CallSucceeds) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        const double startingMin = 900.0;
        const double newMax = 600.0;
        zes_freq_range_t limits;

        pSysfsAccess->setVal(minFreqFile, startingMin);
        // If the new Max value is less than the old Min
        // value, the new Min must be set before the new Max
        limits.min = minFreq;
        limits.max = newMax;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(minFreq, limits.min);
        EXPECT_DOUBLE_EQ(newMax, limits.max);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeThenVerifyzesFrequencySetRangeTest2CallSucceeds) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        const double startingMax = 600.0;
        const double newMin = 900.0;
        zes_freq_range_t limits;

        pSysfsAccess->setVal(maxFreqFile, startingMax);
        // If the new Min value is greater than the old Max
        // value, the new Max must be set before the new Min
        limits.min = newMin;
        limits.max = maxFreq;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(newMin, limits.min);
        EXPECT_DOUBLE_EQ(maxFreq, limits.max);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenInvalidFrequencyLimitsWhenCallingFrequencySetRangeThenVerifyFrequencySetRangeTest1ReturnsError) {
    auto pFrequencyImp = std::make_unique<FrequencyImp>(pOsSysman);
    zes_freq_range_t limits;

    // Verify that Max must be within range.
    limits.min = minFreq;
    limits.max = clockValue(maxFreq + step);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFrequencyImp->frequencySetRange(&limits));
}

TEST_F(SysmanDeviceFrequencyFixture, GivenInvalidFrequencyLimitsWhenCallingFrequencySetRangeThenVerifyFrequencySetRangeTest2ReturnsError) {
    auto pFrequencyImp = std::make_unique<FrequencyImp>(pOsSysman);
    zes_freq_range_t limits;

    // Verify that Min must be within range.
    limits.min = clockValue(minFreq - step);
    limits.max = maxFreq;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFrequencyImp->frequencySetRange(&limits));
}

TEST_F(SysmanDeviceFrequencyFixture, GivenInvalidFrequencyLimitsWhenCallingFrequencySetRangeThenVerifyFrequencySetRangeTest3ReturnsError) {
    auto pFrequencyImp = std::make_unique<FrequencyImp>(pOsSysman);
    zes_freq_range_t limits;

    // Verify that values must be multiples of step.
    limits.min = clockValue(minFreq + (step * 0.5));
    limits.max = maxFreq;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFrequencyImp->frequencySetRange(&limits));
}

TEST_F(SysmanDeviceFrequencyFixture, GivenInvalidFrequencyLimitsWhenCallingFrequencySetRangeThenVerifyFrequencySetRangeTest4ReturnsError) {
    auto pFrequencyImp = std::make_unique<FrequencyImp>(pOsSysman);
    zes_freq_range_t limits;

    // Verify that Max must be greater than min range.
    limits.min = clockValue(maxFreq + step);
    limits.max = minFreq;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFrequencyImp->frequencySetRange(&limits));
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetStateThenVerifyzesFrequencyGetStateTestCallSucceeds) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        const double testRequestValue = 450.0;
        const double testTdpValue = 1200.0;
        const double testEfficientValue = 400.0;
        const double testActualValue = 550.0;
        zes_freq_state_t state;

        pSysfsAccess->setVal(requestFreqFile, testRequestValue);
        pSysfsAccess->setVal(tdpFreqFile, testTdpValue);
        pSysfsAccess->setVal(actualFreqFile, testActualValue);
        pSysfsAccess->setVal(efficientFreqFile, testEfficientValue);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_DOUBLE_EQ(testRequestValue, state.request);
        EXPECT_DOUBLE_EQ(testTdpValue, state.tdp);
        EXPECT_DOUBLE_EQ(testEfficientValue, state.efficient);
        EXPECT_DOUBLE_EQ(testActualValue, state.actual);
        EXPECT_EQ(0u, state.throttleReasons);
        EXPECT_EQ(nullptr, state.pNext);
        EXPECT_EQ(ZES_STRUCTURE_TYPE_FREQ_STATE, state.stype);
        EXPECT_LE(state.currentVoltage, 0);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidStatePointerWhenValidatingfrequencyGetStateForFailuresThenAPIExitsGracefully) {
    auto pFrequencyImp = std::make_unique<FrequencyImp>(pOsSysman);
    zes_freq_state_t state = {};
    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValRequestReturnError));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFrequencyImp->frequencyGetState(&state));

    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValTdpReturnError));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFrequencyImp->frequencyGetState(&state));

    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValEfficientReturnError));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFrequencyImp->frequencyGetState(&state));

    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValActualReturnError));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFrequencyImp->frequencyGetState(&state));
}

TEST_F(SysmanDeviceFrequencyFixture, GivenThrottleTimeStructPointerWhenCallingfrequencyGetThrottleTimeThenUnsupportedIsReturned) {
    auto pFrequencyImp = std::make_unique<FrequencyImp>(pOsSysman);
    zes_freq_throttle_time_t throttleTime = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFrequencyImp->frequencyGetThrottleTime(&throttleTime));
}

class SysmanFrequencyFixture : public DeviceFixture, public ::testing::Test {

  protected:
    std::unique_ptr<SysmanImp> sysmanImp;
    zet_sysman_handle_t hSysman;
    zet_sysman_freq_handle_t hSysmanFrequency;
    Mock<FrequencySysfsAccess> *pSysfsAccess = nullptr;

    OsFrequency *pOsFrequency = nullptr;
    FrequencyImp *pFrequencyImp = nullptr;
    PublicLinuxFrequencyImp linuxFrequencyImp;

    double clockValue(const double calculatedClock) {
        // i915 specific. frequency step is a fraction
        // However, the i915 represents all clock
        // rates as integer values. So clocks are
        // rounded to the nearest integer.
        uint32_t actualClock = static_cast<uint32_t>(calculatedClock + 0.5);
        return static_cast<double>(actualClock);
    }

    void SetUp() override {
        DeviceFixture::SetUp();
        sysmanImp = std::make_unique<SysmanImp>(device->toHandle());
        pSysfsAccess = new NiceMock<Mock<FrequencySysfsAccess>>;
        linuxFrequencyImp.pSysfsAccess = pSysfsAccess;
        pOsFrequency = static_cast<OsFrequency *>(&linuxFrequencyImp);
        pFrequencyImp = new FrequencyImp();
        pFrequencyImp->pOsFrequency = pOsFrequency;
        pSysfsAccess->setVal(minFreqFile, minFreq);
        pSysfsAccess->setVal(maxFreqFile, maxFreq);
        pSysfsAccess->setVal(requestFreqFile, request);
        pSysfsAccess->setVal(tdpFreqFile, tdp);
        pSysfsAccess->setVal(actualFreqFile, actual);
        pSysfsAccess->setVal(efficientFreqFile, efficient);
        pSysfsAccess->setVal(maxValFreqFile, maxVal);
        pSysfsAccess->setVal(minValFreqFile, minVal);
        ON_CALL(*pSysfsAccess, read(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<FrequencySysfsAccess>::getVal));
        ON_CALL(*pSysfsAccess, write(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<FrequencySysfsAccess>::setVal));
        pFrequencyImp->init();
        sysmanImp->pFrequencyHandleContext->handle_list.push_back(pFrequencyImp);
        hSysman = sysmanImp->toHandle();
        hSysmanFrequency = pFrequencyImp->toHandle();
    }

    void TearDown() override {
        pFrequencyImp->pOsFrequency = nullptr;

        if (pSysfsAccess != nullptr) {
            delete pSysfsAccess;
            pSysfsAccess = nullptr;
        }
        DeviceFixture::TearDown();
    }
};

TEST_F(SysmanFrequencyFixture, GivenComponentCountZeroWhenCallingzetSysmanFrequencyGetThenNonZeroCountIsReturnedAndVerifyzetSysmanFrequencyGetCallSucceds) {
    zet_sysman_freq_handle_t freqHandle;
    uint32_t count;

    ze_result_t result = zetSysmanFrequencyGet(hSysman, &count, NULL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_GT(count, 0u);

    uint32_t testCount = count + 1;
    result = zetSysmanFrequencyGet(hSysman, &testCount, NULL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testCount, count);

    result = zetSysmanFrequencyGet(hSysman, &count, &freqHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(freqHandle, nullptr);
    EXPECT_GT(count, 0u);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencyGetPropertiesThenVerifyzetSysmanFrequencyGetPropertiesCallSucceeds) {
    zet_freq_properties_t properties;

    ze_result_t result = zetSysmanFrequencyGetProperties(hSysmanFrequency, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_FREQ_DOMAIN_GPU, properties.type);
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_DOUBLE_EQ(maxFreq, properties.max);
    EXPECT_DOUBLE_EQ(minFreq, properties.min);
    EXPECT_TRUE(properties.canControl);
    EXPECT_DOUBLE_EQ(step, properties.step);
    ASSERT_NE(0.0, properties.step);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleAndZeroCountWhenCallingzetSysmanFrequencyGetAvailableClocksCallSucceeds) {
    uint32_t count = 0;

    ze_result_t result = zetSysmanFrequencyGetAvailableClocks(hSysmanFrequency, &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numClocks, count);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleAndCorrectCountWhenCallingzetSysmanFrequencyGetAvailableClocksCallSucceeds) {
    uint32_t count = 0;

    ze_result_t result = zetSysmanFrequencyGetAvailableClocks(hSysmanFrequency, &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numClocks, count);

    double *clocks = new double[count];
    result = zetSysmanFrequencyGetAvailableClocks(hSysmanFrequency, &count, clocks);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numClocks, count);
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_DOUBLE_EQ(clockValue(minFreq + (step * i)), clocks[i]);
    }
    delete[] clocks;
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencyGetRangeThenVerifyzetSysmanFrequencyGetRangeTestCallSucceeds) {
    zet_freq_range_t limits;

    ze_result_t result = zetSysmanFrequencyGetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_DOUBLE_EQ(minFreq, limits.min);
    EXPECT_DOUBLE_EQ(maxFreq, limits.max);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencySetRangeThenVerifyzetSysmanFrequencySetRangeTest1CallSucceeds) {
    const double startingMin = 900.0;
    const double newMax = 600.0;

    zet_freq_range_t limits;

    pSysfsAccess->setVal(minFreqFile, startingMin);

    // If the new Max value is less than the old Min
    // value, the new Min must be set before the new Max

    limits.min = minFreq;
    limits.max = newMax;
    ze_result_t result = zetSysmanFrequencySetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zetSysmanFrequencyGetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_DOUBLE_EQ(minFreq, limits.min);
    EXPECT_DOUBLE_EQ(newMax, limits.max);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencySetRangeThenVerifyzetSysmanFrequencySetRangeTest2CallSucceeds) {
    const double startingMax = 600.0;
    const double newMin = 900.0;

    zet_freq_range_t limits;

    pSysfsAccess->setVal(maxFreqFile, startingMax);

    // If the new Min value is greater than the old Max
    // value, the new Max must be set before the new Min

    limits.min = newMin;
    limits.max = maxFreq;
    ze_result_t result = zetSysmanFrequencySetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zetSysmanFrequencyGetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_DOUBLE_EQ(newMin, limits.min);
    EXPECT_DOUBLE_EQ(maxFreq, limits.max);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencySetRangeThenVerifyzetSysmanFrequencySetRangeTest3CallSucceeds) {
    zet_freq_range_t limits;

    // Verify that Max must be within range.
    limits.min = minFreq;
    limits.max = clockValue(maxFreq + step);
    ze_result_t result = zetSysmanFrequencySetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencySetRangeThenVerifyzetSysmanFrequencySetRangeTest4CallSucceeds) {
    zet_freq_range_t limits;

    // Verify that Min must be within range.
    limits.min = clockValue(minFreq - step);
    limits.max = maxFreq;
    ze_result_t result = zetSysmanFrequencySetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencySetRangeThenVerifyzetSysmanFrequencySetRangeTest5CallSucceeds) {
    zet_freq_range_t limits;

    // Verify that values must be multiples of step.
    limits.min = clockValue(minFreq + (step * 0.5));
    limits.max = maxFreq;
    ze_result_t result = zetSysmanFrequencySetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencyGetStateThenVerifyzetSysmanFrequencyGetStateTestCallSucceeds) {
    const double testRequestValue = 450.0;
    const double testTdpValue = 1200.0;
    const double testEfficientValue = 400.0;
    const double testActualValue = 550.0;

    zet_freq_state_t state;

    pSysfsAccess->setVal(requestFreqFile, testRequestValue);
    pSysfsAccess->setVal(tdpFreqFile, testTdpValue);
    pSysfsAccess->setVal(actualFreqFile, testActualValue);
    pSysfsAccess->setVal(efficientFreqFile, testEfficientValue);

    ze_result_t result = zetSysmanFrequencyGetState(hSysmanFrequency, &state);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_DOUBLE_EQ(testRequestValue, state.request);
    EXPECT_DOUBLE_EQ(testTdpValue, state.tdp);
    EXPECT_DOUBLE_EQ(testEfficientValue, state.efficient);
    EXPECT_DOUBLE_EQ(testActualValue, state.actual);
    EXPECT_EQ(0u, state.throttleReasons);
}

} // namespace ult
} // namespace L0