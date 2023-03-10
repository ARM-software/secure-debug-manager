// ext_com_port_driver_test.cpp
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "ext_com_port_driver.h"

using namespace testing;

namespace
{
    class ExternalComPortDriverTest : public TestWithParam<SDMDebugArchitectureEnum>
    {
    public:
        ExternalComPortDriverTest()
        {
        }

        virtual void SetUp()
        {
            comDevice.deviceType = SDMDeviceType_ArmADI_CoreSightComponent;
            comDevice.armCoreSightComponent.dpIndex = 0;
            comDevice.armCoreSightComponent.memAp = NULL;
            comDevice.armCoreSightComponent.baseAddress = 0x12345678;
        }

    protected:
        void testInit(Sequence&, ExternalComPortDriver&);

        void ExpectGetStatus(Sequence&, bool ready = true);
        void ExpectSendFlag(Sequence&, uint32_t&, uint8_t);
        void ExpectWaitFlag(Sequence&, uint8_t);
        void ExpectWaitFlagNull(Sequence&);
        void ExpectRxInt(Sequence&, uint8_t*, size_t);
        void ExpectTx(Sequence&, uint8_t*, size_t);

        const int con = 0xABCDABCD;
        void* refcon = (void*) &con;

        using MockSDMRegisterAccessCallback = testing::MockFunction<SDMReturnCode(const SDMDeviceDescriptor *, SDMTransferSize, const SDMRegisterAccess *, size_t, size_t *, void *)>;
        MockSDMRegisterAccessCallback mockRegAccessCallback;

        using MockSDMResetCallback = testing::MockFunction<SDMReturnCode(SDMResetType, void *)>;
        MockSDMResetCallback mockResetStartCallback;
        MockSDMResetCallback mockResetEndCallback;

        SDMDeviceDescriptor comDevice;
    };
}

INSTANTIATE_TEST_SUITE_P(ExternalComPortDriverArchTest, ExternalComPortDriverTest,
        testing::Values(0, 1));

ACTION(SDMRegisterAccessSetAccessesComplete)
{
    size_t length = arg3;
    size_t *accessesCompleted = arg4;
    *accessesCompleted = length;
}

ACTION_P(SDMRegisterAccessSetValue, value)
{
    size_t length = arg3;
    const SDMRegisterAccess* accesses = arg2;

    for (int i = 0; i < length; i++)
    {
        *(accesses[i].value) = value;
    }

    size_t *accessesCompleted = arg4;
    *accessesCompleted = length;
}

bool operator==(const SDMDeviceDescriptor& lhs, const SDMDeviceDescriptor& rhs)
{
    if (lhs.deviceType != rhs.deviceType)
    {
        return false;
    }

    if (lhs.deviceType == SDMDeviceType_ArmADI_AP)
    {
        return lhs.armAP.dpIndex == rhs.armAP.dpIndex &&
            lhs.armAP.address == rhs.armAP.address;
    }
    else if (lhs.deviceType == SDMDeviceType_ArmADI_CoreSightComponent)
    {
        // both have a parent MEM-AP
        if (lhs.armCoreSightComponent.memAp != NULL && rhs.armCoreSightComponent.memAp != NULL)
        {
            return lhs.armCoreSightComponent.dpIndex == rhs.armCoreSightComponent.dpIndex &&
                lhs.armCoreSightComponent.memAp->armAP.dpIndex == rhs.armCoreSightComponent.memAp->armAP.dpIndex &&
                lhs.armCoreSightComponent.memAp->armAP.address == rhs.armCoreSightComponent.memAp->armAP.address &&
                lhs.armCoreSightComponent.baseAddress == rhs.armCoreSightComponent.baseAddress;
        }

        // at least one doesn't have a parent MEM-AP
        return lhs.armCoreSightComponent.dpIndex == rhs.armCoreSightComponent.dpIndex &&
            lhs.armCoreSightComponent.memAp == rhs.armCoreSightComponent.memAp &&
            lhs.armCoreSightComponent.baseAddress == rhs.armCoreSightComponent.baseAddress;
    }

    return false;
}

// SDMRegisterAccess comparator
bool operator==(const SDMRegisterAccess& rhs, const SDMRegisterAccess& lhs)
{
    return
        rhs.address == lhs.address &&
        rhs.op == lhs.op &&
        // only check value on write op
        (rhs.op == SDMRegisterAccessOp_Write ? *rhs.value == *lhs.value : true) &&
        rhs.pollMask == lhs.pollMask &&
        rhs.retries == lhs.retries;
}

void ExternalComPortDriverTest::ExpectGetStatus(Sequence& s, bool ready)
{
    // SR register
    const uint64_t regAddr = GetParam() == SDMDebugArchitecture_ArmADIv5 ? 0x2C : 0xD2C;

    static uint32_t registerAccessValue = 0x0;
    SDMRegisterAccess expectedRegisterAccess[1] = {
        {
            regAddr,                  // address
            SDMRegisterAccessOp_Read, // op
            &registerAccessValue,     // value
            0x0,                      // pollMask
            0                         // retriess
        }
    };

    if (ready)
    {
        EXPECT_CALL(mockRegAccessCallback, Call(Pointee(comDevice), _, _, 1, _, refcon))
            .With(Args<2, 3>(ElementsAreArray(expectedRegisterAccess)))
            .Times(Exactly(1))
            .InSequence(s)
            // tx has space free & rx has data
            .WillOnce(DoAll(SDMRegisterAccessSetValue(0x10001), Return(SDMReturnCode_Success)));
    }
    else
    {
        // not ready will poll SR until timeout
        EXPECT_CALL(mockRegAccessCallback, Call(Pointee(comDevice), _, _, 1, _, refcon))
            .With(Args<2, 3>(ElementsAreArray(expectedRegisterAccess)))
            .Times(AtLeast(1))
            .InSequence(s)
            // tx has space free & rx has data
            .WillRepeatedly(DoAll(SDMRegisterAccessSetValue(0x0), Return(SDMReturnCode_Success)));
    }
}

void ExternalComPortDriverTest::ExpectSendFlag(Sequence& s, uint32_t& drValue, uint8_t flag)
{
    ExpectGetStatus(s);

    // DR register
    const uint64_t regAddr = GetParam() == SDMDebugArchitecture_ArmADIv5 ? 0x20 : 0xD20;

    drValue = 0xAFAFAF00 | flag;
    SDMRegisterAccess expectedRegisterAccess[1] = {
        {
            regAddr,                   // address
            SDMRegisterAccessOp_Write, // op
            &drValue,                  // value
            0x0,                       // pollMask
            0                          // retries
        }
    };

    EXPECT_CALL(mockRegAccessCallback, Call(Pointee(comDevice), _, _, 1, _, refcon))
        .With(Args<2,3>(ElementsAreArray(expectedRegisterAccess)))
        .Times(Exactly(1))
        .InSequence(s)
        .WillOnce(DoAll(SDMRegisterAccessSetAccessesComplete(), Return(SDMReturnCode_Success)));
}

void ExternalComPortDriverTest::ExpectWaitFlag(Sequence& s, uint8_t flag)
{
    ExpectGetStatus(s);

    // DR register
    const uint64_t regAddr = GetParam() == SDMDebugArchitecture_ArmADIv5 ? 0x20 : 0xD20;

    static uint32_t registerAccessValue = 0x0;
    SDMRegisterAccess expectedRegisterAccess[1] = {
        {
            regAddr,                  // address
            SDMRegisterAccessOp_Read, // op
            &registerAccessValue,     // value
            0x0,                      // pollMask
            0                         // retriess
        }
    };

    EXPECT_CALL(mockRegAccessCallback, Call(Pointee(comDevice), _, _, 1, _, refcon))
        .With(Args<2, 3>(ElementsAreArray(expectedRegisterAccess)))
        .Times(Exactly(1))
        .InSequence(s)
        .WillOnce(DoAll(SDMRegisterAccessSetValue(0xAFAFAF00 | flag), Return(SDMReturnCode_Success)));
}

void ExternalComPortDriverTest::ExpectRxInt(Sequence& s, uint8_t* data, size_t dataSize)
{
    static const size_t REG_VALUES_LEN = 1024;

    if (dataSize >= REG_VALUES_LEN)
    {
        throw std::logic_error("ExternalComPortDriverTest::ExpectRxInt - dataSize exceeds static buffer");
    }

    static uint32_t registerAccessValues[REG_VALUES_LEN];

    // DR register
    const uint64_t regAddr = GetParam() == SDMDebugArchitecture_ArmADIv5 ? 0x20 : 0xD20;

    for (size_t i = 0; i < dataSize; i++)
    {
        SDMRegisterAccess expectedRegisterAccess[1] = {
            {
                regAddr,                  // address
                SDMRegisterAccessOp_Read, // op
                &registerAccessValues[i], // value
                0x0,                      // pollMask
                0                         // retriess
            }
        };

        EXPECT_CALL(mockRegAccessCallback, Call(Pointee(comDevice), _, _, 1, _, refcon))
            .With(Args<2, 3>(ElementsAreArray(expectedRegisterAccess)))
            .Times(Exactly(1))
            .InSequence(s)
            .WillOnce(DoAll(SDMRegisterAccessSetValue(0xAFAFAF00 | data[i]), Return(SDMReturnCode_Success)));
    } 
}

void ExternalComPortDriverTest::ExpectTx(Sequence& s, uint8_t* data, size_t dataSize)
{
    static const size_t REG_VALUES_LEN = 1024;

    if (dataSize >= REG_VALUES_LEN)
    {
        throw std::logic_error("ExternalComPortDriverTest::ExpectTx - dataSize exceeds static buffer");
    }

    static uint32_t registerAccessValues[REG_VALUES_LEN];

    // DBR register
    const uint64_t regAddr = GetParam() == SDMDebugArchitecture_ArmADIv5 ? 0x30 : 0xD30;

    SDMRegisterAccess expectedRegisterAccess[REG_VALUES_LEN];
    for (size_t i = 0; i < dataSize; i++)
    {
        expectedRegisterAccess[i].address = regAddr;
        expectedRegisterAccess[i].op = SDMRegisterAccessOp_Write;
        expectedRegisterAccess[i].value = &registerAccessValues[i];
        *expectedRegisterAccess[i].value = 0xAFAFAF00 | data[i];
        expectedRegisterAccess[i].pollMask = 0x0;
        expectedRegisterAccess[i].retries = 0;
    }

    EXPECT_CALL(mockRegAccessCallback, Call(Pointee(comDevice), _, _, dataSize, _, refcon))
        .With(Args<2,3>(ElementsAreArray(expectedRegisterAccess, dataSize)))
        .Times(Exactly(1))
        .WillOnce(DoAll(SDMRegisterAccessSetAccessesComplete(), Return(SDMReturnCode_Success)));
}

void ExternalComPortDriverTest::testInit(Sequence& s, ExternalComPortDriver& extCom)
{
    // EComPort_Power -> EComSendFlag(FLAG_LPH1RL)
    uint32_t flagLPH1RL;
    ExpectSendFlag(s, flagLPH1RL, FLAG_LPH1RL);

    // EComPort_Power -> EComWaitFlag(FLAG_LPH1RL)
    ExpectWaitFlag(s, FLAG_LPH1RL);

    // EComPort_Power -> EComSendFlag(FLAG_LPH1RA)
    uint32_t flagLPH1RA;
    ExpectSendFlag(s, flagLPH1RA, FLAG_LPH1RA);

    // EComPort_Power -> EComWaitFlag(FLAG_LPH1RA)
    ExpectWaitFlag(s, FLAG_LPH1RA);

    // EComSendFlag(FLAG_LPH2RA)
    uint32_t flagLPH2RA;
    ExpectSendFlag(s, flagLPH2RA, FLAG_LPH2RA);

    // EComWaitFlag(FLAG_LPH2RA)
    ExpectWaitFlag(s, FLAG_LPH2RA);

    // EComSendFlag(FLAG_IDR)
    uint32_t flagIDR;
    ExpectSendFlag(s, flagIDR, FLAG_IDR);

    // EComRxIn(FLAG_IDA -> 6 bytes -> FLAG_END)
    uint8_t dataIDA[] = {
        FLAG_IDA, 0x12, 0x34, 0x56,
        0x78, 0x9A, 0xBC, FLAG_END
    };
    ExpectRxInt(s, dataIDA, 8);

    static const size_t SD_RESPONSE_LENGTH = 6;
    uint8_t idResBuff[SD_RESPONSE_LENGTH];
    EXPECT_EQ(SDMReturnCode_Success, extCom.EComPort_Init(ECPD_REMOTE_RESET_NONE, idResBuff, SD_RESPONSE_LENGTH));

    uint8_t expectedID[] = {
        0x12, 0x34, 0x56, 0x78,
        0x9A, 0xBC
    };
    EXPECT_THAT(idResBuff, ElementsAreArray(expectedID));
}

TEST_P(ExternalComPortDriverTest, EComPort_Init_NoReset)
{
    ExternalComPortDriver extCom(comDevice, GetParam(), mockRegAccessCallback.AsStdFunction(), mockResetStartCallback.AsStdFunction(), mockResetEndCallback.AsStdFunction(), refcon);

    Sequence s;

    testInit(s, extCom);
}

TEST_P(ExternalComPortDriverTest, EComPort_Init_RemoteReboot)
{
    ExternalComPortDriver extCom(comDevice, GetParam(), mockRegAccessCallback.AsStdFunction(), mockResetStartCallback.AsStdFunction(), mockResetEndCallback.AsStdFunction(), refcon);

    Sequence s1;

    // EComPort_Power -> EComSendFlag(FLAG_LPH1RL)
    uint32_t flagLPH1RL;
    ExpectSendFlag(s1, flagLPH1RL, FLAG_LPH1RL);

    // EComPort_Power -> EComWaitFlag(FLAG_LPH1RL)
    ExpectWaitFlag(s1, FLAG_LPH1RL);

    // EComPort_Power -> EComSendFlag(FLAG_LPH1RA)
    uint32_t flagLPH1RA;
    ExpectSendFlag(s1, flagLPH1RA, FLAG_LPH1RA);

    // EComPort_Power -> EComWaitFlag(FLAG_LPH1RA)
    ExpectWaitFlag(s1, FLAG_LPH1RA);

    // EComSendFlag(FLAG_LPH2RA)
    uint32_t flagLPH2RA;
    ExpectSendFlag(s1, flagLPH2RA, FLAG_LPH2RA);

    // EComSendFlag(FLAG_LPH2RR)
    uint32_t flagLPH2RR;
    ExpectSendFlag(s1, flagLPH2RR, FLAG_LPH2RR);

    // EComWaitFlag(FLAG_LPH2RA)
    ExpectWaitFlag(s1, FLAG_LPH2RA);

    // EComSendFlag(FLAG_IDR)
    uint32_t flagIDR;
    ExpectSendFlag(s1, flagIDR, FLAG_IDR);

    // EComRxIn(FLAG_IDA -> 6 bytes -> FLAG_END)
    uint8_t dataIDA[] = {
        FLAG_IDA, 0x12, 0x34, 0x56,
        0x78, 0x9A, 0xBC, FLAG_END
    };
    ExpectRxInt(s1, dataIDA, 8);

    static const size_t SD_RESPONSE_LENGTH = 6;
    uint8_t idResBuff[SD_RESPONSE_LENGTH];
    EXPECT_EQ(SDMReturnCode_Success, extCom.EComPort_Init(ECPD_REMOTE_RESET_COM, idResBuff, SD_RESPONSE_LENGTH));

    uint8_t expectedID[] = {
        0x12, 0x34, 0x56, 0x78,
        0x9A, 0xBC
    };
    EXPECT_THAT(idResBuff, ElementsAreArray(expectedID));
}

TEST_P(ExternalComPortDriverTest, EComPort_Init_SystemReset)
{
    ExternalComPortDriver extCom(comDevice, GetParam(), mockRegAccessCallback.AsStdFunction(), mockResetStartCallback.AsStdFunction(), mockResetEndCallback.AsStdFunction(), refcon);

    Sequence s1;

    // SRST start
    EXPECT_CALL(mockResetStartCallback, Call(SDMResetType_Default, refcon))
        .Times(Exactly(1))
        .InSequence(s1)
        .WillOnce(Return(SDMReturnCode_Success));

    // EComPort_Power -> EComSendFlag(FLAG_LPH1RL)
    uint32_t flagLPH1RL;
    ExpectSendFlag(s1, flagLPH1RL, FLAG_LPH1RL);

    // EComPort_Power -> EComWaitFlag(FLAG_LPH1RL)
    ExpectWaitFlag(s1, FLAG_LPH1RL);

    // EComPort_Power -> EComSendFlag(FLAG_LPH1RA)
    uint32_t flagLPH1RA;
    ExpectSendFlag(s1, flagLPH1RA, FLAG_LPH1RA);

    // EComPort_Power -> EComWaitFlag(FLAG_LPH1RA)
    ExpectWaitFlag(s1, FLAG_LPH1RA);

    // EComSendFlag(FLAG_LPH2RA)
    uint32_t flagLPH2RA;
    ExpectSendFlag(s1, flagLPH2RA, FLAG_LPH2RA);

    // SRST start
    EXPECT_CALL(mockResetEndCallback, Call(SDMResetType_Default, refcon))
        .Times(Exactly(1))
        .InSequence(s1)
        .WillOnce(Return(SDMReturnCode_Success));

    // EComWaitFlag(FLAG_LPH2RA)
    ExpectWaitFlag(s1, FLAG_LPH2RA);

    // EComSendFlag(FLAG_IDR)
    uint32_t flagIDR;
    ExpectSendFlag(s1, flagIDR, FLAG_IDR);

    // EComRxIn(FLAG_IDA -> 6 bytes -> FLAG_END)
    uint8_t dataIDA[] = {
        FLAG_IDA, 0x12, 0x34, 0x56,
        0x78, 0x9A, 0xBC, FLAG_END
    };
    ExpectRxInt(s1, dataIDA, 8);

    static const size_t SD_RESPONSE_LENGTH = 6;
    uint8_t idResBuff[SD_RESPONSE_LENGTH];
    EXPECT_EQ(SDMReturnCode_Success, extCom.EComPort_Init(ECPD_REMOTE_RESET_SYSTEM, idResBuff, SD_RESPONSE_LENGTH));

    uint8_t expectedID[] = {
        0x12, 0x34, 0x56, 0x78,
        0x9A, 0xBC
    };
    EXPECT_THAT(idResBuff, ElementsAreArray(expectedID));
}

TEST_P(ExternalComPortDriverTest, EComPort_Init_Timeout)
{
    ExternalComPortDriver extCom(comDevice, GetParam(), mockRegAccessCallback.AsStdFunction(), mockResetStartCallback.AsStdFunction(), mockResetEndCallback.AsStdFunction(), refcon);

    Sequence s1;

    // EComPort_Power -> EComSendFlag(FLAG_LPH1RL)
    uint32_t flagLPH1RL;
    ExpectSendFlag(s1, flagLPH1RL, FLAG_LPH1RL);

    // EComPort_Power -> EComWaitFlag(FLAG_LPH1RL)
    ExpectWaitFlag(s1, FLAG_LPH1RL);

    // EComPort_Power -> EComSendFlag(FLAG_LPH1RA)
    uint32_t flagLPH1RA;
    ExpectSendFlag(s1, flagLPH1RA, FLAG_LPH1RA);

    // EComPort_Power -> EComWaitFlag(FLAG_LPH1RA)
    ExpectWaitFlag(s1, FLAG_LPH1RA);

    // EComSendFlag(FLAG_LPH2RA)
    uint32_t flagLPH2RA;
    ExpectSendFlag(s1, flagLPH2RA, FLAG_LPH2RA);

    // EComWaitFlag(FLAG_LPH2RA) - timeout
    ExpectGetStatus(s1, false);

    static const size_t SD_RESPONSE_LENGTH = 6;
    uint8_t idResBuff[SD_RESPONSE_LENGTH];
    EXPECT_EQ(SDMReturnCode_TimeoutError, extCom.EComPort_Init(ECPD_REMOTE_RESET_NONE, idResBuff, SD_RESPONSE_LENGTH));
}

TEST_P(ExternalComPortDriverTest, EComPort_Finalize)
{
    ExternalComPortDriver extCom(comDevice, GetParam(), mockRegAccessCallback.AsStdFunction(), mockResetStartCallback.AsStdFunction(), mockResetEndCallback.AsStdFunction(), refcon);

    Sequence s1;

    // EComSendFlag(FLAG_LPH2RL)
    uint32_t flagLPH2RL;
    ExpectSendFlag(s1, flagLPH2RL, FLAG_LPH2RL);

    // EComWaitFlag(FLAG_LPH2RL)
    ExpectWaitFlag(s1, FLAG_LPH2RL);

    // EComPort_Power -> EComSendFlag(FLAG_LPH1RL)
    uint32_t flagLPH1RL;
    ExpectSendFlag(s1, flagLPH1RL, FLAG_LPH1RL);

    // EComPort_Power -> EComWaitFlag(FLAG_LPH1RL)
    ExpectWaitFlag(s1, FLAG_LPH1RL);

    EXPECT_EQ(SDMReturnCode_Success, extCom.EComPort_Finalize());
}

TEST_P(ExternalComPortDriverTest, EComPort_RemoteReboot)
{
    ExternalComPortDriver extCom(comDevice, GetParam(), mockRegAccessCallback.AsStdFunction(), mockResetStartCallback.AsStdFunction(), mockResetEndCallback.AsStdFunction(), refcon);

    Sequence s1;

    // EComPort_RemoteReboot()
    uint32_t flagLPH2RR;
    ExpectSendFlag(s1, flagLPH2RR, FLAG_LPH2RR);

    EXPECT_EQ(SDMReturnCode_Success, extCom.EComPort_RReboot());
}

TEST_P(ExternalComPortDriverTest, EComPort_Tx_NoInit)
{
    ExternalComPortDriver extCom(comDevice, GetParam(), mockRegAccessCallback.AsStdFunction(), mockResetStartCallback.AsStdFunction(), mockResetEndCallback.AsStdFunction(), refcon);

    size_t actualLen = 0;;
    uint8_t data[] = {
        0x12, 0x34, 0x56, 0x78
    };
    EXPECT_EQ(SDMReturnCode_RequestFailed, extCom.EComPort_Tx(data, 4, &actualLen, true));
}

TEST_P(ExternalComPortDriverTest, EComPort_Tx_1)
{
    ExternalComPortDriver extCom(comDevice, GetParam(), mockRegAccessCallback.AsStdFunction(), mockResetStartCallback.AsStdFunction(), mockResetEndCallback.AsStdFunction(), refcon);

    Sequence s;

    testInit(s, extCom);

    uint8_t expectedData[] = {
        // START and END flags added to data
        FLAG_START, 0x12, 0x34, 0x56,
        0x78, FLAG_END
    };
    ExpectTx(s, expectedData, 6);

    size_t actualLen = 0;;
    uint8_t data[] = {
        0x12, 0x34, 0x56, 0x78
    };
    EXPECT_EQ(SDMReturnCode_Success, extCom.EComPort_Tx(data, 4, &actualLen, true));
}

TEST_P(ExternalComPortDriverTest, EComPort_Tx_2)
{
    ExternalComPortDriver extCom(comDevice, GetParam(), mockRegAccessCallback.AsStdFunction(), mockResetStartCallback.AsStdFunction(), mockResetEndCallback.AsStdFunction(), refcon);

    Sequence s;

    testInit(s, extCom);

    uint8_t expectedData[] = {
        // data bytes that match flags are preceded by an ESC flag
        // and bit [7] of the data byte is flipped
        FLAG_START, 0x12, FLAG_ESC, 0x20,
        0x34, FLAG_ESC, 0x22, 0x56,
        FLAG_ESC, 0x2F, 0x78, FLAG_END
    };
    ExpectTx(s, expectedData, 12);

    size_t actualLen = 0;;
    uint8_t data[] = {
        0x12, 0xA0, 0x34, 0xA2,
        0x56, 0xAF, 0x78
    };
    EXPECT_EQ(SDMReturnCode_Success, extCom.EComPort_Tx(data, 7, &actualLen, true));
}

TEST_P(ExternalComPortDriverTest, EComPort_Rx_NoInit)
{
    ExternalComPortDriver extCom(comDevice, GetParam(), mockRegAccessCallback.AsStdFunction(), mockResetStartCallback.AsStdFunction(), mockResetEndCallback.AsStdFunction(), refcon);

    size_t actualLen = 0;;
    uint8_t data[4] = { 0x0 };
    EXPECT_EQ(SDMReturnCode_RequestFailed, extCom.EComPort_Rx(data, 4, &actualLen));
}

TEST_P(ExternalComPortDriverTest, EComPort_Rx_1)
{
    ExternalComPortDriver extCom(comDevice, GetParam(), mockRegAccessCallback.AsStdFunction(), mockResetStartCallback.AsStdFunction(), mockResetEndCallback.AsStdFunction(), refcon);

    Sequence s;

    testInit(s, extCom);

    uint8_t rxData[] = {
        FLAG_START, 0x12, 0x34, 0x56,
        0x78, FLAG_END
    };
    ExpectRxInt(s, rxData, 6);

    size_t actualLen = 0;;
    uint8_t data[4] = { 0x0 };
    EXPECT_EQ(SDMReturnCode_Success, extCom.EComPort_Rx(data, 4, &actualLen));

    uint8_t expectedData[] = {
        0x12, 0x34, 0x56, 0x78
    };
    EXPECT_EQ(4, actualLen);
    EXPECT_THAT(data, ElementsAreArray(expectedData));
}

TEST_P(ExternalComPortDriverTest, EComPort_Rx_2)
{
    ExternalComPortDriver extCom(comDevice, GetParam(), mockRegAccessCallback.AsStdFunction(), mockResetStartCallback.AsStdFunction(), mockResetEndCallback.AsStdFunction(), refcon);

    Sequence s;

    testInit(s, extCom);

    uint8_t rxData[] = {
        FLAG_START, 0x12, FLAG_ESC, 0x20,
        0x34, FLAG_ESC, 0x22, 0x56,
        FLAG_ESC, 0x2F, 0x78, FLAG_END
    };
    ExpectRxInt(s, rxData, 12);

    size_t actualLen = 0;;
    uint8_t data[10] = { 0x0 };
    EXPECT_EQ(SDMReturnCode_Success, extCom.EComPort_Rx(data, 10, &actualLen));

    uint8_t expectedData[] = {
        0x12, 0xA0, 0x34, 0xA2,
        0x56, 0xAF, 0x78, 0x0,
        0x0, 0x0
    };
    EXPECT_EQ(7, actualLen);
    EXPECT_THAT(data, ElementsAreArray(expectedData));
}

TEST_P(ExternalComPortDriverTest, EComPort_Rx_3)
{
    ExternalComPortDriver extCom(comDevice, GetParam(), mockRegAccessCallback.AsStdFunction(), mockResetStartCallback.AsStdFunction(), mockResetEndCallback.AsStdFunction(), refcon);

    Sequence s;

    testInit(s, extCom);

    uint8_t rxData[] = {
        FLAG__NULL, FLAG__NULL, FLAG_START, 0x12,
        0x34, 0x56, 0x78, FLAG_END
    };
    ExpectRxInt(s, rxData, 8);

    size_t actualLen = 0;;
    uint8_t data[4] = { 0x0 };
    EXPECT_EQ(SDMReturnCode_Success, extCom.EComPort_Rx(data, 4, &actualLen));

    uint8_t expectedData[] = {
        0x12, 0x34, 0x56, 0x78
    };
    EXPECT_EQ(4, actualLen);
    EXPECT_THAT(data, ElementsAreArray(expectedData));
}

TEST_P(ExternalComPortDriverTest, EComPort_Rx_BufferTooSmall)
{
    ExternalComPortDriver extCom(comDevice, GetParam(), mockRegAccessCallback.AsStdFunction(), mockResetStartCallback.AsStdFunction(), mockResetEndCallback.AsStdFunction(), refcon);

    Sequence s;

    testInit(s, extCom);

    uint8_t rxData[] = {
        FLAG_START, 0x12, 0x34, 0x56,
        0x78, 0x9A, 0xBC, 0xDE,
        0xFF, FLAG_END
    };
    ExpectRxInt(s, rxData, 6);

    size_t actualLen = 0;;
    uint8_t data[4] = { 0x0 };
    EXPECT_EQ(SDMReturnCode_InternalError, extCom.EComPort_Rx(data, 4, &actualLen));

    // available buffer is still used
    uint8_t expectedData[] = {
        0x12, 0x34, 0x56, 0x78
    };
    EXPECT_EQ(4, actualLen);
    EXPECT_THAT(data, ElementsAreArray(expectedData));
}

TEST_P(ExternalComPortDriverTest, EComPort_Rx_Timeout)
{
    ExternalComPortDriver extCom(comDevice, GetParam(), mockRegAccessCallback.AsStdFunction(), mockResetStartCallback.AsStdFunction(), mockResetEndCallback.AsStdFunction(), refcon);

    Sequence s;

    testInit(s, extCom);

    // repeatedly return NULL
    const uint64_t regAddr = GetParam() == SDMDebugArchitecture_ArmADIv5 ? 0x20 : 0xD20;
    static uint32_t registerAccessValue = 0x0;
    SDMRegisterAccess expectedRegisterAccess[1] = {
        {
            regAddr,                  // address
            SDMRegisterAccessOp_Read, // op
            &registerAccessValue,     // value
            0x0,                      // pollMask
            0                         // retriess
        }
    };
    EXPECT_CALL(mockRegAccessCallback, Call(Pointee(comDevice), _, _, 1, _, refcon))
        .With(Args<2, 3>(ElementsAreArray(expectedRegisterAccess)))
        .Times(AtLeast(1))
        .InSequence(s)
        .WillRepeatedly(DoAll(SDMRegisterAccessSetValue(0xAFAFAFAF), Return(SDMReturnCode_Success)));

    size_t actualLen = 0;;
    uint8_t data[4] = { 0x0 };
    EXPECT_EQ(SDMReturnCode_TimeoutError, extCom.EComPort_Rx(data, 4, &actualLen));
    EXPECT_EQ(0, actualLen);
}

TEST_P(ExternalComPortDriverTest, EComPort_Rx_OutOfOrder)
{
    ExternalComPortDriver extCom(comDevice, GetParam(), mockRegAccessCallback.AsStdFunction(), mockResetStartCallback.AsStdFunction(), mockResetEndCallback.AsStdFunction(), refcon);

    Sequence s;

    testInit(s, extCom);

    uint8_t rxData[] = {
        FLAG__NULL, FLAG__NULL, FLAG_END, 0x12
    };
    ExpectRxInt(s, rxData, 3);

    size_t actualLen = 0;;
    uint8_t data[4] = { 0x0 };
    EXPECT_EQ(SDMReturnCode_InternalError, extCom.EComPort_Rx(data, 4, &actualLen));
    EXPECT_EQ(0, actualLen);
}
