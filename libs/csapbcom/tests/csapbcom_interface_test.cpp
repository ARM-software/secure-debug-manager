// csapbcom_interface_test.cpp
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "rddiwrapper/irddi_wrapper.h"
#include "csapbcom.h"

#include "rddi_debug.h"

using namespace testing;

// We need this because our API is C-like and in order to mock the RDDI dependency,
// we have to inject our mock.
extern void InjectRDDIWrapper(IRDDIWrapper*);

namespace
{
    class MockRDDIWrapper : public IRDDIWrapper
    {
    public:
        MOCK_METHOD2(Open, int(RDDIHandle*, const void*));
        MOCK_METHOD1(Close, int(RDDIHandle));
        MOCK_METHOD3(ConfigInfo_OpenFileAndRetarget, int(RDDIHandle, const char*, const char*));
        MOCK_METHOD8(Debug_Connect, int(RDDIHandle, const char*, char*, size_t, char*, size_t, char*, size_t));
        MOCK_METHOD2(Debug_Disconnect, int(RDDIHandle, int));
        MOCK_METHOD6(Debug_OpenConn, int(RDDIHandle, int, int*, int*, char*, size_t));
        MOCK_METHOD2(Debug_CloseConn, int(RDDIHandle, int));
        MOCK_METHOD4(Debug_SetConfig, int(RDDIHandle, int, char*, char*));
        MOCK_METHOD4(Debug_RegRWList, int(RDDIHandle, int, RDDI_REG_ACC_OP*, size_t));
        MOCK_METHOD6(Debug_RegReadBlock, int(RDDIHandle, int, uint32, int, uint32*, size_t));
        MOCK_METHOD5(Debug_RegWriteBlock, int(RDDIHandle, int, uint32, int, const uint32*));
        MOCK_METHOD3(Debug_SystemReset, int(RDDIHandle, int, int));
    };

    int Helper_SetIncreasingRDDIHandle(RDDIHandle* handle, const void*)
    {
        static RDDIHandle rddiHandle = 1;
        *handle = rddiHandle++;
        return RDDI_SUCCESS;
    }

    class CSAPBCOMInterfaceTest : public Test
    {
    public:
        CSAPBCOMInterfaceTest(){}

        virtual void SetUp()
        {
            connDesc.sdf = "/path/to/sdf_file.sdf";
            connDesc.address = "TCP:MyDSTREAM.example.com";
            connDesc.dapIndex = 3;
            connDesc.deviceIndex = 5;

            defaultRegAccOp.registerID = 0xD20 / 4; //regId  = APBCOM.DR
            defaultRegAccOp.registerSize = 1;       //regSize 1 = 32bit
            defaultRegAccOp.rwFlag = 1;             //rwFlag 1 = write
            defaultRegAccOp.pRegisterValue = NULL;
            defaultRegAccOp.errorCode = 0;
            defaultRegAccOp.errorLength = 0;
            defaultRegAccOp.pErrorMsg = NULL;
        }

        virtual void TearDown()
        {
            InjectRDDIWrapper(NULL);
        }

    protected:
        CSAPBCOMConnectionDescription connDesc;
        RDDI_REG_ACC_OP defaultRegAccOp;

        // create list of RDDI_REG_ACC_OPs and set the values
        void createRegAccOpList(RDDI_REG_ACC_OP* opList, int rwFlag, uint32* values, int count)
        {
            defaultRegAccOp.rwFlag = rwFlag;
            std::fill_n(&opList[0], count, defaultRegAccOp);

            for (int i = 0; i < count; i++)
            {
                opList[i].pRegisterValue = &values[i];
            }
        }
    };
}

void testCSAPBCOM_Open(MockRDDIWrapper* mockWrapper)
{
    EXPECT_CALL(*mockWrapper, Open(_, NULL))
        .Times(Exactly(1))
        .WillOnce(DoAll(SetArgPointee<0>(1), Return(RDDI_SUCCESS)));
    EXPECT_CALL(*mockWrapper, ConfigInfo_OpenFileAndRetarget(_, _, _))
        .Times(Exactly(1))
        .WillOnce(Return(RDDI_SUCCESS));
    EXPECT_CALL(*mockWrapper, Debug_Connect(_, _, _, _, _, _, _, _))
        .Times(Exactly(1))
        .WillOnce(Return(RDDI_SUCCESS));
}

void testCSAPBCOM_Connect(MockRDDIWrapper* mockWrapper, RDDIHandle* rddiHandle, int deviceIndex, uint32 templID)
{
    EXPECT_CALL(*mockWrapper, Debug_OpenConn(*rddiHandle, deviceIndex, _, _, _, _))
        .Times(Exactly(1))
        .WillOnce(DoAll(SetArgPointee<2>(templID), Return(RDDI_SUCCESS)));
}

TEST_F(CSAPBCOMInterfaceTest, ErrorsMatch)
{
    EXPECT_EQ(CSAPBCOM_SUCCESS, RDDI_SUCCESS);
    EXPECT_EQ(CSAPBCOM_BADARG, RDDI_BADARG);
    EXPECT_EQ(CSAPBCOM_INVALID_HANDLE, RDDI_INVHANDLE);
    EXPECT_EQ(CSAPBCOM_FAILED, RDDI_FAILED);
    EXPECT_EQ(CSAPBCOM_TOO_MANY_CONNECTIONS, RDDI_TOOMANYCONNECTIONS);
    EXPECT_EQ(CSAPBCOM_BUFFER_OVERFLOW, RDDI_BUFFER_OVERFLOW);
    EXPECT_EQ(CSAPBCOM_INTERNAL_ERROR, RDDI_INTERNAL_ERROR);
    EXPECT_EQ(CSAPBCOM_PARSE_FAILED, RDDI_PARSE_FAILED);
    EXPECT_EQ(CSAPBCOM_REG_ACCESS, RDDI_REGACCESS);
    EXPECT_EQ(CSAPBCOM_TIMEOUT, RDDI_TIMEOUT);
    EXPECT_EQ(CSAPBCOM_RW_FAIL, RDDI_RWFAIL);
    EXPECT_EQ(CSAPBCOM_DEV_UNKNOWN, RDDI_DEVUNKNOWN);
    EXPECT_EQ(CSAPBCOM_DEV_IN_USE, RDDI_DEVINUSE);
    EXPECT_EQ(CSAPBCOM_NO_CONN, RDDI_NOCONN);
    EXPECT_EQ(CSAPBCOM_COMMS, RDDI_COMMS);
    EXPECT_EQ(CSAPBCOM_DEV_BUSY, RDDI_DEVBUSY);
    EXPECT_EQ(CSAPBCOM_NO_INIT, RDDI_NOINIT);
    EXPECT_EQ(CSAPBCOM_LOST_CONN, RDDI_LOSTCONN);
    EXPECT_EQ(CSAPBCOM_NO_VCC, RDDI_NOVCC);
    EXPECT_EQ(CSAPBCOM_NO_RESPONSE, RDDI_NORESPONSE);
    EXPECT_EQ(CSAPBCOM_OUT_OF_MEM, RDDI_OUTOFMEM);
    EXPECT_EQ(CSAPBCOM_WRONG_DEV, RDDI_WRONGDEV);
    EXPECT_EQ(CSAPBCOM_NODEBUGPOWER, RDDI_NODEBUGPOWER);
    EXPECT_EQ(CSAPBCOM_UNKNOWN, RDDI_UNKNOWN);
    EXPECT_EQ(CSAPBCOM_NO_CONFIG_FILE, RDDI_NO_CONFIG_FILE);
    EXPECT_EQ(CSAPBCOM_UNKNOWN_CONFIG, RDDI_UNKNOWN_CONFIG);
}

TEST_F(CSAPBCOMInterfaceTest, GetInterfaceVersionReturnsValidString)
{
    char versionDetails[128];
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_GetInterfaceVersion(versionDetails,
                sizeof(versionDetails)));

    EXPECT_THAT("CSAPBCOM RDDI V3.0", StrEq(versionDetails));
}

TEST_F(CSAPBCOMInterfaceTest, GetInterfaceVersionErrorsOnBadArgs)
{
    char versionDetails[128];
    EXPECT_EQ(CSAPBCOM_BADARG, CSAPBCOM_GetInterfaceVersion(NULL, 128));
    EXPECT_EQ(CSAPBCOM_BADARG, CSAPBCOM_GetInterfaceVersion(versionDetails, 0));
}

TEST_F(CSAPBCOMInterfaceTest, GetInterfaceVersionTruncatesAndErrorsOnBufferOverflow)
{
    char versionDetails[4];
    EXPECT_EQ(CSAPBCOM_BUFFER_OVERFLOW, CSAPBCOM_GetInterfaceVersion(versionDetails,
                sizeof(versionDetails)));

    EXPECT_EQ(0, versionDetails[3]);
    EXPECT_THAT("CSA", StrEq(versionDetails));
}

TEST_F(CSAPBCOMInterfaceTest, OpensRDDIAndConnectsToDebugVehicle)
{
    // Inject wrapper
    MockRDDIWrapper mockWrapper;
    InjectRDDIWrapper(&mockWrapper);

    RDDIHandle rddiHandle = 1;
    EXPECT_CALL(mockWrapper, Open(_, NULL))
        .Times(Exactly(1))
        .WillOnce(DoAll(SetArgPointee<0>(rddiHandle), Return(RDDI_SUCCESS)));

    EXPECT_CALL(mockWrapper, ConfigInfo_OpenFileAndRetarget(rddiHandle,
        StrEq("/path/to/sdf_file.sdf"), StrEq("TCP:MyDSTREAM.example.com")))
            .Times(Exactly(1));

    EXPECT_CALL(mockWrapper, Debug_Connect(rddiHandle, StrEq("CSAPBCOM_connection"), _, _, _, _, _, _))
        .Times(Exactly(1));

    CSAPBCOMHandle handle;
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
}

TEST_F(CSAPBCOMInterfaceTest, SuccessfulMultipleOpens)
{
    // Inject wrapper
    MockRDDIWrapper mockWrapper;
    InjectRDDIWrapper(&mockWrapper);

    EXPECT_CALL(mockWrapper, Open(_, NULL))
        .Times(Exactly(3))
        .WillRepeatedly(Invoke(Helper_SetIncreasingRDDIHandle));

    EXPECT_CALL(mockWrapper, ConfigInfo_OpenFileAndRetarget(_, _, _))
        .Times(Exactly(3));
    EXPECT_CALL(mockWrapper, Debug_Connect(_, _, _, _, _, _, _, _))
        .Times(Exactly(3));

    CSAPBCOMHandle handleA, handleB, handleC;
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handleA, &connDesc));
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handleB, &connDesc));
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handleC, &connDesc));

    EXPECT_NE(handleA, handleB);
    EXPECT_NE(handleA, handleC);
    EXPECT_NE(handleB, handleC);
}

TEST_F(CSAPBCOMInterfaceTest, CapturesAndReturnsRDDIErrors_Open)
{
    // Inject wrapper
    MockRDDIWrapper mockWrapper;
    InjectRDDIWrapper(&mockWrapper);

    CSAPBCOMHandle handle;

    // RDDI_Open - return error
    {
        EXPECT_CALL(mockWrapper, Open(_, NULL))
            .Times(Exactly(1))
            .WillOnce(DoAll(SetArgPointee<0>(0xFFFF), Return(RDDI_TOOMANYCONNECTIONS)));
        EXPECT_CALL(mockWrapper, Close(_))
            .Times(Exactly(0));
        EXPECT_CALL(mockWrapper, ConfigInfo_OpenFileAndRetarget(_, _, _))
            .Times(Exactly(0));
        EXPECT_CALL(mockWrapper, Debug_Connect(_, _, _, _, _, _, _, _))
            .Times(Exactly(0));

        EXPECT_EQ(CSAPBCOM_TOO_MANY_CONNECTIONS, CSAPBCOM_Open(&handle, &connDesc));
    }

    // ConfigInfo_OpenFileAndRetarget - return error
    {
        EXPECT_CALL(mockWrapper, Open(_, NULL))
            .Times(Exactly(1))
            .WillOnce(DoAll(SetArgPointee<0>(1), Return(RDDI_SUCCESS)));
        EXPECT_CALL(mockWrapper, Close(_))
            .Times(Exactly(1));
        EXPECT_CALL(mockWrapper, ConfigInfo_OpenFileAndRetarget(_, _, _))
            .Times(Exactly(1))
            .WillOnce(Return(RDDI_BADARG));
        EXPECT_CALL(mockWrapper, Debug_Connect(_, _, _, _, _, _, _, _))
            .Times(Exactly(0));

        EXPECT_EQ(CSAPBCOM_BADARG, CSAPBCOM_Open(&handle, &connDesc));
    }

    // Debug_Connect - return error
    {
        EXPECT_CALL(mockWrapper, Open(_, NULL))
            .Times(Exactly(1))
            .WillOnce(DoAll(SetArgPointee<0>(1), Return(RDDI_SUCCESS)));
        EXPECT_CALL(mockWrapper, Close(_))
            .Times(Exactly(1));
        EXPECT_CALL(mockWrapper, ConfigInfo_OpenFileAndRetarget(_, _, _))
            .Times(Exactly(1))
            .WillOnce(Return(RDDI_SUCCESS));
        EXPECT_CALL(mockWrapper, Debug_Connect(_, _, _, _, _, _, _, _))
            .Times(Exactly(1))
            .WillOnce(Return(RDDI_NOVCC));

        EXPECT_EQ(CSAPBCOM_NO_VCC, CSAPBCOM_Open(&handle, &connDesc));
    }
}

TEST_F(CSAPBCOMInterfaceTest, OpenErrorsOnNullArgs)
{
    CSAPBCOMHandle h;
    EXPECT_EQ(CSAPBCOM_BADARG, CSAPBCOM_Open(NULL, &connDesc));
    EXPECT_EQ(CSAPBCOM_BADARG, CSAPBCOM_Open(&h, NULL));
}

TEST_F(CSAPBCOMInterfaceTest, ClosesRDDIConnectionAndDisconnects)
{
    // Inject wrapper
    MockRDDIWrapper mockWrapper;
    InjectRDDIWrapper(&mockWrapper);

    testCSAPBCOM_Open(&mockWrapper);

    {
        InSequence seq;

        EXPECT_CALL(mockWrapper, Debug_Disconnect(1, 0))
            .Times(Exactly(1));

        EXPECT_CALL(mockWrapper, Close(1))
            .Times(Exactly(1));
    }

    CSAPBCOMHandle handle;

    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Close(handle));
}

TEST_F(CSAPBCOMInterfaceTest, CloseRemovesMappedHandles)
{
    CSAPBCOMHandle handle;

    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Close(handle));
    EXPECT_EQ(CSAPBCOM_INVALID_HANDLE, CSAPBCOM_Close(handle));
}

TEST_F(CSAPBCOMInterfaceTest, SetsErrorOnCloseIfNotOpened)
{
    EXPECT_EQ(CSAPBCOM_INTERNAL_ERROR, CSAPBCOM_Close(1));
}

TEST_F(CSAPBCOMInterfaceTest, SetsErrorOnCloseIfHandleIsInvalid)
{
    CSAPBCOMHandle handle;

    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_INVALID_HANDLE, CSAPBCOM_Close(1));
}

TEST_F(CSAPBCOMInterfaceTest, CapturesAndReturnsRDDIErrors_Close)
{
    // Inject wrapper
    MockRDDIWrapper mockWrapper;
    InjectRDDIWrapper(&mockWrapper);

    // Debug_Disconnect - return error
    {
        testCSAPBCOM_Open(&mockWrapper);

        // Close() functionality
        EXPECT_CALL(mockWrapper, Debug_Disconnect(_, _))
            .Times(Exactly(1))
            .WillOnce(Return(RDDI_FAILED));
        EXPECT_CALL(mockWrapper, Close(_))
            .Times(Exactly(0));

        CSAPBCOMHandle handle;

        EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
        EXPECT_EQ(CSAPBCOM_FAILED, CSAPBCOM_Close(handle));
    }

    // RDDI_Close - return error
    {
        testCSAPBCOM_Open(&mockWrapper);

        // Close() functionality
        EXPECT_CALL(mockWrapper, Debug_Disconnect(_, _))
            .Times(Exactly(1))
            .WillOnce(Return(RDDI_SUCCESS));
        EXPECT_CALL(mockWrapper, Close(_))
            .Times(Exactly(1))
            .WillOnce(Return(RDDI_OUTOFMEM));

        CSAPBCOMHandle handle;

        EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
        EXPECT_EQ(CSAPBCOM_OUT_OF_MEM, CSAPBCOM_Close(handle));
    }
}

TEST_F(CSAPBCOMInterfaceTest, ConnectsToCOMAP)
{
    // Inject wrapper
    MockRDDIWrapper mockWrapper;
    InjectRDDIWrapper(&mockWrapper);

    //Open
    RDDIHandle rddiHandle = 1;
    testCSAPBCOM_Open(&mockWrapper);

    // 0x04762000 - COM-AP IDR value
    testCSAPBCOM_Connect(&mockWrapper, &rddiHandle, connDesc.deviceIndex, 0x04762000);

    CSAPBCOMHandle handle;

    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Connect(handle));
}

TEST_F(CSAPBCOMInterfaceTest, ConnectsToAPBCOM)
{
    // Inject wrapper
    MockRDDIWrapper mockWrapper;
    InjectRDDIWrapper(&mockWrapper);

    //Open
    RDDIHandle rddiHandle = 1;
    testCSAPBCOM_Open(&mockWrapper);

    // 0x9ef - APBCOM pID
    testCSAPBCOM_Connect(&mockWrapper, &rddiHandle, connDesc.deviceIndex, 0x9ef);

    CSAPBCOMHandle handle;

    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Connect(handle));
}

TEST_F(CSAPBCOMInterfaceTest, ErrorOnConnectToInvalidDevice)
{
    // Inject wrapper
    MockRDDIWrapper mockWrapper;
    InjectRDDIWrapper(&mockWrapper);

    // Open
    RDDIHandle rddiHandle = 1;
    testCSAPBCOM_Open(&mockWrapper);

    // Connect to invalid deivde (Not COM-AP or APBCOM) - invalid ID returned by template 
    EXPECT_CALL(mockWrapper, Debug_OpenConn(rddiHandle, connDesc.deviceIndex, _, _, _, _))
        .Times(Exactly(1))
        .WillOnce(DoAll(SetArgPointee<2>(0xABCD), Return(RDDI_SUCCESS)));

    // Disconnect
    EXPECT_CALL(mockWrapper, Debug_CloseConn(rddiHandle, connDesc.deviceIndex))
        .Times(Exactly(1))
        .WillOnce(Return(RDDI_SUCCESS));

    CSAPBCOMHandle handle;

    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_WRONG_DEV, CSAPBCOM_Connect(handle));
}

TEST_F(CSAPBCOMInterfaceTest, SetsErrorOnConnectfNotOpened)
{
    EXPECT_EQ(CSAPBCOM_INTERNAL_ERROR, CSAPBCOM_Connect(1));
}

TEST_F(CSAPBCOMInterfaceTest, SetsErrorOnConnectfHandleIsInvalid)
{
    CSAPBCOMHandle handle;

    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_INVALID_HANDLE, CSAPBCOM_Connect(1));
}

TEST_F(CSAPBCOMInterfaceTest, DisconnectsFromDevice)
{
    // Inject wrapper
    MockRDDIWrapper mockWrapper;
    InjectRDDIWrapper(&mockWrapper);

    // Open
    RDDIHandle rddiHandle = 1;
    testCSAPBCOM_Open(&mockWrapper);

    // Connect
    testCSAPBCOM_Connect(&mockWrapper, &rddiHandle, connDesc.deviceIndex, 0x04762000);

    // Disconnect
    EXPECT_CALL(mockWrapper, Debug_CloseConn(rddiHandle, connDesc.deviceIndex))
        .Times(Exactly(1))
        .WillOnce(Return(RDDI_SUCCESS));

    CSAPBCOMHandle handle;

    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Connect(handle));
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Disconnect(handle));
}

TEST_F(CSAPBCOMInterfaceTest, SetsErrorOnDisconnectfNotOpened)
{
    EXPECT_EQ(CSAPBCOM_INTERNAL_ERROR, CSAPBCOM_Disconnect(1));
}

TEST_F(CSAPBCOMInterfaceTest, SetsErrorOnDisconnectfHandleIsInvalid)
{
    CSAPBCOMHandle handle;

    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_INVALID_HANDLE, CSAPBCOM_Disconnect(1));
}

ACTION_P(RegRWListSetValuesAndIncrement, startValue)
{
    size_t length = arg3;
    RDDI_REG_ACC_OP* accOpList = arg2;

    for (int i = 0; i < length; i++)
    {
        *(accOpList[i].pRegisterValue) = (*startValue)++;
    }
}

TEST_F(CSAPBCOMInterfaceTest, ReadData)
{
    // Inject wrapper
    MockRDDIWrapper mockWrapper;
    InjectRDDIWrapper(&mockWrapper);

    const int bufferLen = 8;
    uint8 values[bufferLen];

    const int expectedValuesA[bufferLen] = { 0x1, 0x2, 0x3, 0x4, 0x0, 0x0, 0x0, 0x0 };
    const int expectedValuesB[bufferLen] = { 0x5, 0x6, 0x7, 0x0, 0x0, 0x0, 0x0, 0x0 };
    const int expectedValuesC[bufferLen] = { 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF };

    // initial value for setting read values with RegRWListSetValuesAndIncrement
    uint32 readRegVal = 0x1;

    // Open
    RDDIHandle rddiHandle = 1;
    testCSAPBCOM_Open(&mockWrapper);

    // Connect to APBCOM
    testCSAPBCOM_Connect(&mockWrapper, &rddiHandle, connDesc.deviceIndex, 0x9ef);

    // pRegisterValues are not matched as the the out params on reads
    // just used to initialise the REG_ACC_OP list
    uint32 outVals[8] = { 0 };

    // single word read
    RDDI_REG_ACC_OP expectedOpListA[4];
    createRegAccOpList(expectedOpListA, 0, outVals, 4);
    EXPECT_CALL(mockWrapper, Debug_RegRWList(rddiHandle, connDesc.deviceIndex, _, 4))
        .With(Args<2, 3>(ElementsAreArray(expectedOpListA)))
        .Times(Exactly(1))
        .WillOnce(DoAll(RegRWListSetValuesAndIncrement(&readRegVal) , Return(RDDI_SUCCESS)));

    // non-word aligned read
    RDDI_REG_ACC_OP expectedOpListB[3];
    createRegAccOpList(expectedOpListB, 0, outVals, 3);
    EXPECT_CALL(mockWrapper, Debug_RegRWList(rddiHandle, connDesc.deviceIndex, _, 3))
        .With(Args<2, 3>(ElementsAreArray(expectedOpListB)))
        .Times(Exactly(1))
        .WillOnce(DoAll(RegRWListSetValuesAndIncrement(&readRegVal), Return(RDDI_SUCCESS)));

    // double word read
    RDDI_REG_ACC_OP expectedOpListC[8];
    createRegAccOpList(expectedOpListC, 0, outVals, 8);
    EXPECT_CALL(mockWrapper, Debug_RegRWList(rddiHandle, connDesc.deviceIndex, _, 8))
        .With(Args<2, 3>(ElementsAreArray(expectedOpListC)))
        .Times(Exactly(1))
        .WillOnce(DoAll(RegRWListSetValuesAndIncrement(&readRegVal), Return(RDDI_SUCCESS)));

    CSAPBCOMHandle handle;

    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Connect(handle));

    // single word
    memset(values, 0x0, sizeof(values));
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_ReadData(handle, 4, values, bufferLen));
    EXPECT_THAT(values, ElementsAreArray(expectedValuesA));

    // non word-aligned
    memset(values, 0x0, sizeof(values));
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_ReadData(handle, 3, values, bufferLen));
    EXPECT_THAT(values, ElementsAreArray(expectedValuesB));

    // doubel word
    memset(values, 0x0, sizeof(values));
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_ReadData(handle, 8, values, bufferLen));
    EXPECT_THAT(values, ElementsAreArray(expectedValuesC));
}

TEST_F(CSAPBCOMInterfaceTest, SetsErrorOnReadDataIfNotOpened)
{
    const int numBytes = 4;
    unsigned char data[numBytes];
    EXPECT_EQ(CSAPBCOM_INTERNAL_ERROR, CSAPBCOM_ReadData(1, numBytes, data, numBytes));
}

TEST_F(CSAPBCOMInterfaceTest, SetsErrorOnReadDataIfHandleIsInvalid)
{
    CSAPBCOMHandle handle;
    const int numBytes = 4;
    unsigned char data[numBytes];

    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_INVALID_HANDLE, CSAPBCOM_ReadData(1, numBytes, data, numBytes));
}

// RDDI_REG_ACC_OP comparator
bool operator==(const RDDI_REG_ACC_OP& rhs, const RDDI_REG_ACC_OP& lhs)
{
    return
        rhs.registerID == lhs.registerID &&
        rhs.registerSize == lhs.registerSize &&
        rhs.rwFlag == lhs.rwFlag &&
        *rhs.pRegisterValue == *lhs.pRegisterValue;
}

TEST_F(CSAPBCOMInterfaceTest, WriteData)
{
    // Inject wrapper
    MockRDDIWrapper mockWrapper;
    InjectRDDIWrapper(&mockWrapper);

    uint8 valuesA[] = { 0x1, 0x2, 0x3, 0x4 };
    uint8 valuesB[] = { 0x5, 0x6, 0x7 };
    uint8 valuesC[] = { 0xA, 0xB, 0xC, 0xD, 0xA, 0xB, 0xC, 0xD };

    // Open
    RDDIHandle rddiHandle = 1;
    testCSAPBCOM_Open(&mockWrapper);

    // Connect to APBCOM
    testCSAPBCOM_Connect(&mockWrapper, &rddiHandle, connDesc.deviceIndex, 0x9ef);

    // single word write
    uint32 expectedValsA[] = { 0xAFAFAF01, 0xAFAFAF02, 0xAFAFAF03, 0xAFAFAF04 };
    RDDI_REG_ACC_OP expectedOpListA[4];
    createRegAccOpList(expectedOpListA, 1, expectedValsA, 4);
    EXPECT_CALL(mockWrapper, Debug_RegRWList(rddiHandle, connDesc.deviceIndex, _, 4))
        .With(Args<2,3>(ElementsAreArray(expectedOpListA)))
        .Times(Exactly(1))
        .WillOnce(Return(RDDI_SUCCESS));

    // non word-aligned write
    uint32 expectedValsB[] = { 0xAFAFAF05, 0xAFAFAF06, 0xAFAFAF07 };
    RDDI_REG_ACC_OP expectedOpListB[3];
    createRegAccOpList(expectedOpListB, 1, expectedValsB, 3);
    EXPECT_CALL(mockWrapper, Debug_RegRWList(rddiHandle, connDesc.deviceIndex, _, 3))
        .With(Args<2, 3>(ElementsAreArray(expectedOpListB)))
        .Times(Exactly(1))
        .WillOnce(Return(RDDI_SUCCESS));

    // double word write
    uint32 expectedValsC[] = {0xAFAFAF0A, 0xAFAFAF0B, 0xAFAFAF0C, 0xAFAFAF0D, 0xAFAFAF0A, 0xAFAFAF0B, 0xAFAFAF0C, 0xAFAFAF0D};
    RDDI_REG_ACC_OP expectedOpListC[8];
    createRegAccOpList(expectedOpListC, 1, expectedValsC, 8);
    EXPECT_CALL(mockWrapper, Debug_RegRWList(rddiHandle, connDesc.deviceIndex, _, 8))
        .With(Args<2, 3>(ElementsAreArray(expectedOpListC)))
        .Times(Exactly(1))
        .WillRepeatedly(Return(RDDI_SUCCESS));

    CSAPBCOMHandle handle;

    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Connect(handle));

    // single word
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_WriteData(handle, false, 4, valuesA));

    // non word-aligned
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_WriteData(handle, false, 3, valuesB));

    // double word
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_WriteData(handle, false, 8, valuesC));
}

TEST_F(CSAPBCOMInterfaceTest, SetsErrorOnWriteDataIfNotOpened)
{
    uint32 data = 0xAC1DCAFE;
    EXPECT_EQ(CSAPBCOM_INTERNAL_ERROR, CSAPBCOM_WriteData(1, false, sizeof(data), (uint8*)&data));
}

TEST_F(CSAPBCOMInterfaceTest, SetsErrorOnWriteDataIfHandleIsInvalid)
{
    CSAPBCOMHandle handle;

    uint32 data = 0xAC1DCAFE;
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_INVALID_HANDLE, CSAPBCOM_WriteData(1, false, sizeof(data), (uint8*)&data));
}

TEST_F(CSAPBCOMInterfaceTest, PerformSystemReset)
{
    // Inject wrapper
    MockRDDIWrapper mockWrapper;
    InjectRDDIWrapper(&mockWrapper);

    // Open
    RDDIHandle rddiHandle = 1;
    testCSAPBCOM_Open(&mockWrapper);

    // DAP device connection
    EXPECT_CALL(mockWrapper, Debug_OpenConn(rddiHandle, connDesc.dapIndex, _, _, _, _))
        .Times(Exactly(2))
        .WillRepeatedly(Return(RDDI_SUCCESS));
    EXPECT_CALL(mockWrapper, Debug_CloseConn(rddiHandle, connDesc.dapIndex))
        .Times(Exactly(2))
        .WillRepeatedly(Return(RDDI_SUCCESS));

    ON_CALL(mockWrapper, Debug_RegReadBlock(rddiHandle, connDesc.dapIndex, 0x2081, 1, _, 1))
        .WillByDefault(DoAll(SetArgPointee<4>(0xF0000040), Return(RDDI_SUCCESS)));

    // Reset waveform
    {
        InSequence seq;

        // Make sure we read-modify-write
        EXPECT_CALL(mockWrapper, Debug_RegReadBlock(rddiHandle, connDesc.dapIndex,
                    0x2081, 1, _, 1))
            .Times(Exactly(1));

        // Set DAP DBG & SYS REQ bit low
        EXPECT_CALL(mockWrapper, Debug_RegWriteBlock(rddiHandle, connDesc.dapIndex,
                    0x2081, 1, Pointee(0xA0000040)))
            .Times(Exactly(1))
            .WillOnce(Return(RDDI_SUCCESS));

        // Wait the ACK bits to be set
        EXPECT_CALL(mockWrapper, Debug_RegReadBlock(rddiHandle, connDesc.dapIndex,
                    0x2081, 1, _, 1))
            .Times(AtLeast(1))
            .WillRepeatedly(DoAll(SetArgPointee<4>(0x00000040), Return(RDDI_SUCCESS)));

        // ASSERT nSRST
        EXPECT_CALL(mockWrapper, Debug_SystemReset(rddiHandle, 0, RDDI_RST_ASSERT))
            .Times(Exactly(1))
            .WillOnce(Return(RDDI_SUCCESS));

        // Set DAP DBG & SYS REQ bit high
        EXPECT_CALL(mockWrapper, Debug_RegWriteBlock(rddiHandle, connDesc.dapIndex,
                    0x2081, 1, Pointee(0x50000000)))
            .Times(Exactly(1))
            .WillOnce(Return(RDDI_SUCCESS));

        // Wait the ACK bits to be set
        EXPECT_CALL(mockWrapper, Debug_RegReadBlock(rddiHandle, connDesc.dapIndex,
                    0x2081, 1, _, 1))
            .Times(AtLeast(1))
            .WillRepeatedly(DoAll(SetArgPointee<4>(0xF0000000), Return(RDDI_SUCCESS)));

        // Finally, DEASSERT
        EXPECT_CALL(mockWrapper, Debug_SystemReset(rddiHandle, 0, RDDI_RST_DEASSERT))
            .Times(Exactly(1))
            .WillOnce(Return(RDDI_SUCCESS));
    }

    CSAPBCOMHandle handle;

    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_SystemReset(handle, CSAPBCOM_RESET_BEGIN));
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_SystemReset(handle, CSAPBCOM_RESET_END));
}

TEST_F(CSAPBCOMInterfaceTest, HandlesUnableToPowerUpDAP)
{
    // Inject wrapper
    MockRDDIWrapper mockWrapper;
    InjectRDDIWrapper(&mockWrapper);

    // Open
    RDDIHandle rddiHandle = 1;
    testCSAPBCOM_Open(&mockWrapper);

    // DAP device connection
    EXPECT_CALL(mockWrapper, Debug_OpenConn(rddiHandle, connDesc.dapIndex, _, _, _, _))
        .Times(Exactly(1))
        .WillOnce(Return(RDDI_SUCCESS));
    EXPECT_CALL(mockWrapper, Debug_CloseConn(rddiHandle, connDesc.dapIndex))
        .Times(Exactly(1))
        .WillOnce(Return(RDDI_SUCCESS));

    // Reset waveform
    {
        InSequence seq;

        // Make sure we read-modify-write
        EXPECT_CALL(mockWrapper, Debug_RegReadBlock(rddiHandle, connDesc.dapIndex,
            0x2081, 1, _, 1))
            .Times(Exactly(1))
            .WillOnce(DoAll(SetArgPointee<4>(0xF0000040), Return(RDDI_SUCCESS)));

        // Power down the DBG & SYS
        EXPECT_CALL(mockWrapper, Debug_RegWriteBlock(rddiHandle, connDesc.dapIndex,
            0x2081, 1, Pointee(0xA0000040)))
            .Times(Exactly(1))
            .WillOnce(Return(RDDI_SUCCESS));

        // Do not set the ACK bits, to make sure we handle this correctly.
        EXPECT_CALL(mockWrapper, Debug_RegReadBlock(rddiHandle, connDesc.dapIndex,
            0x2081, 1, _, 1))
            .Times(AtLeast(1))
            .WillRepeatedly(DoAll(SetArgPointee<4>(0xA0000040), Return(RDDI_SUCCESS)));

        // ASSERT should not be hit if we fail to power up the DAP
        EXPECT_CALL(mockWrapper, Debug_SystemReset(rddiHandle, 0, RDDI_RST_ASSERT))
            .Times(Exactly(0));
    }

    CSAPBCOMHandle handle;

    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_FAILED, CSAPBCOM_SystemReset(handle, CSAPBCOM_RESET_BEGIN));
}

TEST_F(CSAPBCOMInterfaceTest, SetsErrorOnSRSTIfNotOpened)
{
    EXPECT_EQ(CSAPBCOM_INTERNAL_ERROR, CSAPBCOM_SystemReset(1, CSAPBCOM_RESET_BEGIN));
}

TEST_F(CSAPBCOMInterfaceTest, SetsErrorOnSRSTIfHandleIsInvalid)
{
    CSAPBCOMHandle handle;

    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_INVALID_HANDLE, CSAPBCOM_SystemReset(1, CSAPBCOM_RESET_BEGIN));
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Close(handle));
}

TEST_F(CSAPBCOMInterfaceTest, SetsErrorOnSRSTIfNoDAP)
{
    CSAPBCOMHandle handle;

    connDesc.dapIndex = -1;
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_WRONG_DEV, CSAPBCOM_SystemReset(handle, CSAPBCOM_RESET_BEGIN));
}

TEST_F(CSAPBCOMInterfaceTest, PerformGetStatus)
{
    // Inject wrapper
    MockRDDIWrapper mockWrapper;
    InjectRDDIWrapper(&mockWrapper);

    // Open
    RDDIHandle rddiHandle = 1;
    testCSAPBCOM_Open(&mockWrapper);

    // Connect to APBCOM
    EXPECT_CALL(mockWrapper, Debug_OpenConn(rddiHandle, connDesc.deviceIndex, _, _, _, _))
        .Times(Exactly(1))
        .WillOnce(DoAll(SetArgPointee<2>(0x9ef), Return(RDDI_SUCCESS)));

    // SR reads
    EXPECT_CALL(mockWrapper, Debug_RegReadBlock(rddiHandle, connDesc.deviceIndex, (0xD00 + 0x2C) / 4, 1, _, 1))
        .Times(Exactly(4))
        // tx has space free
        .WillOnce(DoAll(SetArgPointee<4>(0x1), Return(RDDI_SUCCESS)))
        // tx has space free & rx has data to recv
        .WillOnce(DoAll(SetArgPointee<4>(0x10001), Return(RDDI_SUCCESS)))
        // link errors
        .WillOnce(DoAll(SetArgPointee<4>(0x4000), Return(RDDI_SUCCESS)))
        // tx overflow has occured
        .WillOnce(DoAll(SetArgPointee<4>(0x2000), Return(RDDI_SUCCESS)));

    CSAPBCOMHandle handle;
    uint8 txFree, rxData, txOverflow, linkErrs;

    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Connect(handle));

    // tx has space free
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_GetStatus(handle, &txFree, &txOverflow, &rxData, &linkErrs));
    EXPECT_EQ(txFree, 1);
    EXPECT_EQ(txOverflow, 0);
    EXPECT_EQ(rxData, 0);
    EXPECT_EQ(linkErrs, 0);

    // tx has space free & rx has data to recv
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_GetStatus(handle, &txFree, &txOverflow, &rxData, &linkErrs));
    EXPECT_EQ(txFree, 1);
    EXPECT_EQ(txOverflow, 0);
    EXPECT_EQ(rxData, 1);
    EXPECT_EQ(linkErrs, 0);

    // link errors
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_GetStatus(handle, &txFree, &txOverflow, &rxData, &linkErrs));
    EXPECT_EQ(txFree, 0);
    EXPECT_EQ(txOverflow, 0);
    EXPECT_EQ(rxData, 0);
    EXPECT_EQ(linkErrs, 1);

    // tx overflow has occured - NULL params
    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_GetStatus(handle, NULL, &txOverflow, NULL, NULL));
    EXPECT_EQ(txFree, 0);
    EXPECT_EQ(txOverflow, 1);
    EXPECT_EQ(rxData, 0);
    EXPECT_EQ(linkErrs, 1); // remains unchanged
}

TEST_F(CSAPBCOMInterfaceTest, SetsErrorOnGetStatusIfInvalidHandle)
{
    CSAPBCOMHandle handle;
    uint8 txFree, rxData, txOverflow, linkErrs;

    EXPECT_EQ(CSAPBCOM_SUCCESS, CSAPBCOM_Open(&handle, &connDesc));
    EXPECT_EQ(CSAPBCOM_INVALID_HANDLE, CSAPBCOM_GetStatus(1, &txFree, &txOverflow, &rxData, &linkErrs));
}

TEST_F(CSAPBCOMInterfaceTest, SetsErrorOnGetStatusIfNotOpened)
{
    uint8 txFree, rxData, txOverflow, linkErrs;
    EXPECT_EQ(CSAPBCOM_INTERNAL_ERROR, CSAPBCOM_GetStatus(1, &txFree, &txOverflow, &rxData, &linkErrs));
}
