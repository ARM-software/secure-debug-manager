// ext_com_port_driver.cpp
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#include "ext_com_port_driver.h"
#include "sdm_config.h"
#include "psa_adac_debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/time.h>
#endif

#include <algorithm>
#include <vector>

#define ENTITY_NAME "ExternalComPortDriver"

#define REG_DR  0x20
#define REG_SR  0x2C
#define REG_DBR 0x30

#define REG_BASE_ADIv5 0x0
#define REG_BASE_ADIv6 0xD00

/******************************************************************************************************
 *
 * private
 *
 ******************************************************************************************************/
const char* ExternalComPortDriver::apbcomflagToStr(uint8_t flag)
{
#define FLAG_TO_STR(_a) case _a: return #_a
    switch (flag) {
        FLAG_TO_STR(FLAG_IDR    );
        FLAG_TO_STR(FLAG_IDA    );
        FLAG_TO_STR(FLAG_LPH1RA );
        FLAG_TO_STR(FLAG_LPH1RL );
        FLAG_TO_STR(FLAG_LPH2RA );
        FLAG_TO_STR(FLAG_LPH2RL );
        FLAG_TO_STR(FLAG_LPH2RR );
        FLAG_TO_STR(FLAG_START  );
        FLAG_TO_STR(FLAG_END    );
        FLAG_TO_STR(FLAG_ESC    );
        FLAG_TO_STR(FLAG__NULL  );
        default: return "other";
    }
#undef FLAG_TO_STR
}

SDMReturnCode ExternalComPortDriver::EComSendByte(uint8_t byte)
{
    uint8_t txFree = 0;
    uint8_t txOverflow = 0;
    uint8_t linkErrs = 0;

    static const uint32_t MAX_NUM_OF_RETRIES = 5000;
    uint32_t numOfRetries = 0;

    uint8_t txData[] = { byte };

    /* wait for TXS byte value to indicate that the TX FIFO is not full */
    do
    {
        SDMReturnCode result = EComStatus(&txFree, &txOverflow, NULL, &linkErrs);
        if (result != SDMReturnCode_Success)
        {
            PSA_ADAC_LOG_ERR(ENTITY_NAME, "EComStatus failed with code: 0x%x\n", result);
            return result;
        }

        if (linkErrs != 0)
        {
            PSA_ADAC_LOG_ERR(ENTITY_NAME, "EComStatus linkErrs[0x%08x]\n", linkErrs);
            return SDMReturnCode_IOError;
        }

        if (txOverflow != 0)
        {
            PSA_ADAC_LOG_ERR(ENTITY_NAME, "EComStatus txOverflow[0x%08x]\n", txOverflow);
            return SDMReturnCode_IOError;
        }
    }
    while (txFree == 0 && numOfRetries++  < MAX_NUM_OF_RETRIES);

    if (numOfRetries >= MAX_NUM_OF_RETRIES)
    {
        return SDMReturnCode_TimeoutError;
    }

    // write byte to TX
    SDMReturnCode result = EComTxRaw(false, 1, txData);
    if (result != SDMReturnCode_Success)
    {
        PSA_ADAC_LOG_ERR(ENTITY_NAME, "EComTxRaw failed with code: 0x%x\n", result);
        return result;
    }

    return SDMReturnCode_Success;
}

SDMReturnCode ExternalComPortDriver::EComSendBlock(uint8_t* byteData, size_t dataLen, bool block)
{
    if (block)
    {
        SDMReturnCode result = EComTxRaw(true, dataLen, byteData);
        if (result != SDMReturnCode_Success)
        {
            PSA_ADAC_LOG_ERR(ENTITY_NAME, "EComTxRaw failed with code: 0x%x\n", result);
            return result;
        }
    }
    else
    {
        for (int i = 0; i < dataLen; i++)
        {
            SDMReturnCode result = EComSendByte(byteData[i]);
            if (result != SDMReturnCode_Success)
            {
                PSA_ADAC_LOG_ERR(ENTITY_NAME, "EComSendByte failed with code: 0x%x\n", result);
                return result;
            }
        }
    }

    return SDMReturnCode_Success;
}

SDMReturnCode ExternalComPortDriver::EComReadByte(uint8_t* byte)
{
    uint8_t txOverflow = 0;
    uint8_t rxStatusData = 0;
    uint8_t linkErrs = 0;

    const uint32_t MAX_ATTEMPTS = 5000;
    uint32_t attempt = 0;

    uint8_t rxData = 0x0;

    do
    {
        SDMReturnCode result = EComStatus(NULL, &txOverflow, &rxStatusData, &linkErrs);
        if (result != SDMReturnCode_Success)
        {
            PSA_ADAC_LOG_ERR(ENTITY_NAME, "EComStatus failed with code: 0x%x\n", result);
            return result;
        }

        if (linkErrs != 0)
        {
            PSA_ADAC_LOG_ERR(ENTITY_NAME, "EComStatus linkErrs[0x%08x]\n", linkErrs);
            return SDMReturnCode_IOError;
        }

        if (txOverflow != 0)
        {
            PSA_ADAC_LOG_ERR(ENTITY_NAME, "EComStatus txOverflow[0x%08x]\n", txOverflow);
            return SDMReturnCode_IOError;
        }
    }
    while (rxStatusData == 0 && attempt++ < MAX_ATTEMPTS);

    if (attempt >= MAX_ATTEMPTS)
    {
        return SDMReturnCode_TimeoutError;
    }

    SDMReturnCode result = EComRxRaw(sizeof(rxData), &rxData, sizeof(rxData));
    if (result != SDMReturnCode_Success)
    {
        PSA_ADAC_LOG_ERR(ENTITY_NAME, "EComRxRaw failed with code: 0x%x\n", result);
        return result;
    }

    *byte = rxData;

    return SDMReturnCode_Success;
}

SDMReturnCode ExternalComPortDriver::EComSendFlag(uint8_t flag, const char* flag_name)
{
    PSA_ADAC_LOG_INFO("--------->", "%s\n", flag_name);

    return EComSendByte(flag);
}

SDMReturnCode ExternalComPortDriver::EComWaitFlag(uint8_t flag, const char* flag_name)
{
    uint8_t byte = FLAG__NULL;

    PSA_ADAC_LOG_DEBUG(ENTITY_NAME, "waiting for flag[%s]\n", apbcomflagToStr(flag));

    do
    {
        SDMReturnCode result = EComReadByte(&byte);
        if (result != SDMReturnCode_Success)
        {
            return result;
        }
    }
    while (byte != flag);

    PSA_ADAC_LOG_INFO("<---------", "%s\n", flag_name);

    return SDMReturnCode_Success;
}


SDMReturnCode ExternalComPortDriver::EComPortPrepareData(uint8_t startFlag, const uint8_t* data, size_t inSize, uint8_t* outData, size_t outDataBufferSize, size_t* outSize)
{
    uint8_t *tempData = (uint8_t*)calloc(1, (inSize * 2) + 2);
    if (!tempData)
    {
        return SDMReturnCode_InternalError;
    }

    uint16_t outByteIndex = 0;
    tempData[outByteIndex++] = FLAG_START;

    for (uint32_t byte_index = 0; byte_index < inSize; ++byte_index)
    {
        /* Each Message byte that matches one of the Flag bytes is
         * immediately preceded by the ESC Flag byte, and bit [7] of the Message byte is inverted. */
        if (data[byte_index] >= 0xA0 && data[byte_index] < 0xC0)
        {
            tempData[outByteIndex++] = FLAG_ESC;
            tempData[outByteIndex++] = data[byte_index] & ~0x80UL;
        }
        else
        {
            tempData[outByteIndex++] = data[byte_index];
        }
    }

    tempData[outByteIndex++] = FLAG_END;

    if (outByteIndex > outDataBufferSize)
    {
        *outSize = 0;
        PSA_ADAC_LOG_ERR(ENTITY_NAME,
                    "tempData buffer is too small, outByteIndex[%u] outDataBufferSize[%zu]\n",
                    outByteIndex,
                    outDataBufferSize);
        free(tempData);
        return SDMReturnCode_InternalError;
    }
    else
    {
        memcpy(outData, tempData, outByteIndex);
        *outSize = outByteIndex;
    }

    free(tempData);
    return SDMReturnCode_Success;
}

SDMReturnCode ExternalComPortDriver::EComPortRxInt(uint8_t startFlag, uint8_t* rxBuffer, size_t rxBufferLength, size_t* actualLength)
{
    static const uint32_t MAX_PRE_NULL_FLAGS = 10000;
    uint32_t preNullFlags = 0;

    uint8_t read_byte = 0;
    bool is_done = false;
    uint16_t buffer_idx = 0;
    bool isStartRecv = false;
    bool isEndRecv = false;
    bool isEscRecv = false;

    while (is_done == false)
    {
        /* if this fails it means the buffer is empty */
        SDMReturnCode result = EComRxRaw(sizeof(uint8_t), &read_byte, sizeof(uint8_t));
        if (result != SDMReturnCode_Success)
        {
            PSA_ADAC_LOG_ERR(ENTITY_NAME, "EComRxRaw failed with code: 0x%x\n", result);
            return result;
        }

        if (read_byte == FLAG_END)
        {
            is_done = true;
            isEndRecv = true;
            continue;
        }
        else if (read_byte == FLAG__NULL)
        {
            if (!isStartRecv)
                preNullFlags++;

            if (preNullFlags > MAX_PRE_NULL_FLAGS)
                return SDMReturnCode_TimeoutError;

            continue;
        }
        else if (read_byte == FLAG_ESC)
        {
            isEscRecv = true;
        }
        else if (read_byte == startFlag)
        {
            buffer_idx = 0;
            isStartRecv = true;
        }
        else
        {
            if (isEscRecv == true)
            {
                read_byte |= 0x80UL;
                isEscRecv = false;
            }

            if (buffer_idx >= rxBufferLength)
            {
                PSA_ADAC_LOG_ERR(ENTITY_NAME, "RxBufferLength[%u] buffer_idx[%u]\n", (uint32_t)rxBufferLength, buffer_idx);
                return SDMReturnCode_InternalError;
            }

            rxBuffer[buffer_idx] = read_byte;

            buffer_idx += 1;
            *actualLength = buffer_idx;
        }
    }

    PSA_ADAC_LOG_DUMP("  <-----  ", "data_recv", rxBuffer, *actualLength);

    return (isStartRecv ^ isEndRecv) ? SDMReturnCode_InternalError : SDMReturnCode_Success;
}

/******************************************************************************************************
 *
 * public
 *
 ******************************************************************************************************/

ExternalComPortDriver::ExternalComPortDriver(SDMDeviceDescriptor device, SDMDebugArchitecture arch, SDMRegisterAccessCallback registerAccess, SDMResetCallback resetStart, SDMResetCallback resetEnd, void *refcon) :
    mIsComPortInited(false),
    mComDevice(device),
    mRegisterAccessCallback(registerAccess),
    mResetStartCallback(resetStart),
    mResetEndCallback(resetEnd),
    mRefcon(refcon),
    mComDeviceRegisterBase(REG_BASE_ADIv6)
{
    if (arch == SDMDebugArchitecture_ArmADIv5)
    {
        mComDeviceRegisterBase = REG_BASE_ADIv5;
    }

    // If present, copy mComDevice.armCoreSightComponent.memAp to be referenced locally
    if (mComDevice.deviceType == SDMDeviceType_ArmADI_CoreSightComponent && mComDevice.armCoreSightComponent.memAp != NULL)
    {
        mComDeviceAp = *mComDevice.armCoreSightComponent.memAp;
        mComDevice.armCoreSightComponent.memAp = &mComDeviceAp;
    }
}

ExternalComPortDriver::~ExternalComPortDriver()
{
}

SDMReturnCode ExternalComPortDriver::EComPort_Init(ECPDRemoteResetType remoteReset, uint8_t* IDResponseBuffer, size_t IDBufferLength)
{
    SDMReturnCode res = SDMReturnCode_Success;
    size_t actualLength = 0;
    bool isTimeout = false;

    if (remoteReset == ECPD_REMOTE_RESET_SYSTEM)
    {
        PSA_ADAC_ASSERT_ERROR((bool)mResetStartCallback, true, SDMReturnCode_InternalError);
        PSA_ADAC_ASSERT_ERROR((bool)mResetEndCallback, true, SDMReturnCode_InternalError);
        PSA_ADAC_ASSERT(mResetStartCallback(SDMResetType_Default, mRefcon), SDMReturnCode_Success);
    }

    // Setup the Internal COM Port’s power
    // 2.  External COM Port driver calls EComPort_Power(PowerOn)
    // In case of bad status, return with an error.
    PSA_ADAC_ASSERT(EComPort_Power(ECPD_POWER_ON), SDMReturnCode_Success);

    // Establish the link:
    // 3.  Transmits LPH2RA flag to the External COM port TX. External COM port HW will set the LINKEST signal to the Internal COM Port and drop the flag.
    // In case of bad status, return with an error.
    PSA_ADAC_ASSERT(EComSendFlag(FLAG_LPH2RA, "LPH2RA"), SDMReturnCode_Success);
    
    if (remoteReset == ECPD_REMOTE_RESET_COM)
    {
        PSA_ADAC_ASSERT(EComPort_RReboot(), SDMReturnCode_Success);
    }
    else if (remoteReset == ECPD_REMOTE_RESET_SYSTEM)
    {
        PSA_ADAC_ASSERT(mResetEndCallback(SDMResetType_Default, mRefcon), SDMReturnCode_Success);
    }

    // Internal COM Port activity upon power on reset, or if the External COM Port caused Remote Reboot:
    // 5.  The Internal COM Port device HW detects the status of the LINKEST signal.
    //     If it is set to 1, the HW inserts LPH2RA flag to the Internal COM Port’s RX FIFO,
    //     otherwise the RX FIFO remains empty. If LPH2RA symbol is inserted to the RX FIFO then
    //     the Internal COM Port device interrupts its driver (in the ROM case, interrupts are not enabled).
    //     Note: if the Internal COM Port CPU is asleep, this interrupt is assumed to power it up.

    // 6.  If the External COM Port driver did not set the LINKEST to 1 then the Internal COM
    //     Port does not insert any symbol to its RX FIFO. Its device driver finds that the RX FIFO
    //     is empty (RXF field of the Status Register is 0) and understands that the link is not established
    //     from the External COM Port and resumes its normal boot flow (e.g. skips the secure debug handling opportunity).
    // 7.  If the debugger set the LINKEST to 1 at the External COM Port then the Internal COM Port
    //     device driver reads the LPH2RA flag from the device and detects that the link is established
    //     from the External COM Port. At this point the Internal COM Port device driver can initialize
    //     (or restore in case the Internal COM port was powered off while the debugged system was running)
    //     its device context (e.g. set its TX and RX thresholds as required).
    // 8.  The Internal COM Port device driver writes LPH2RA flag to the Internal COM Port device TX.
    // 9.  The Internal COM Port translates the LPH2RA flag into a latched LINKEST=1 signal.
    //     LPH2RA flag is not inserted to the TX FIFO.

    // 10. The External COM Port detects that LINKEST signal is set to 1.
    //     As a result, the HW inserts LPH2RA flag to the External COM Port’s RX FIFO.
    PSA_ADAC_ASSERT(EComWaitFlag(FLAG_LPH2RA, "LPH2RA"), SDMReturnCode_Success);

    // 11. The External COM Port driver polls its RX FIFO to detect LPH2RA flag.
    //     In case of timeout it returns with link establishment timeout expired error.
    // 12. The debugger knows now that the link from the Internal COM Port to the External COM Port is established.
    //
    // External COM Port driver checks the debugged system protocol:
    // 13. The debugger transmits now an IDR flag - Identification Request (note: this is a single flag message with no START or END).
    PSA_ADAC_ASSERT(EComSendFlag(FLAG_IDR, "IDR"), SDMReturnCode_Success);

    // 14. The debugged system IComPortInit() function responds and transmits to the debugger with Identification response message. Note: this response message format has a special format. It starts with IDA flag, followed by 6 bytes of debugged system ID hex value, and an END flag. If any of the platform ID bytes has MS bits value of 101b then the transmit driver must send an ESC flag following a flip of the MS bit of the byte to transmit.
    PSA_ADAC_ASSERT(EComPortRxInt(FLAG_IDA, IDResponseBuffer, IDBufferLength, &actualLength), SDMReturnCode_Success);
    PSA_ADAC_ASSERT_ERROR(actualLength == 0, false, SDMReturnCode_TransferError);
    PSA_ADAC_LOG_DUMP("<---------", "IDResponseBuffer", IDResponseBuffer, actualLength);

    // 15. At this point the debugged system () API returns with success code.
    // 16. The debugger EComPort_Init() API saves the received platform ID (6IComPortInit bytes) in the provided buffer and returns with success code.

    mIsComPortInited = true;

bail:
    return res;
}

SDMReturnCode ExternalComPortDriver::EComPort_Power(ECPDRequiredState RequiredState)
{
    SDMReturnCode res = SDMReturnCode_Success;

    if (RequiredState == ECPD_POWER_ON)
    {
        // Release link first to get it into a know state
        // write LPH1RL to TX
        PSA_ADAC_ASSERT(EComSendFlag(FLAG_LPH1RL, "LPH1RL"), SDMReturnCode_Success);

        // poll for LPH1RL to in RX
        PSA_ADAC_ASSERT(EComWaitFlag(FLAG_LPH1RL, "LPH1RL"), SDMReturnCode_Success);

        // write LPH1RA to TX
        PSA_ADAC_ASSERT(EComSendFlag(FLAG_LPH1RA, "LPH1RA"), SDMReturnCode_Success);

        // poll for LPH1RA to in RX
        PSA_ADAC_ASSERT(EComWaitFlag(FLAG_LPH1RA, "LPH1RA"), SDMReturnCode_Success);
    }

    if (RequiredState == ECPD_POWER_OFF)
    {
        // Release link first to get it into a know state
        // write LPH1RL to TX
        PSA_ADAC_ASSERT(EComSendFlag(FLAG_LPH1RL, "LPH1RL"), SDMReturnCode_Success);

        // poll for LPH1RL to in RX
        PSA_ADAC_ASSERT(EComWaitFlag(FLAG_LPH1RL, "LPH1RL"), SDMReturnCode_Success);
    }

bail:
    return res;
}

SDMReturnCode ExternalComPortDriver::EComPort_RReboot(void)
{
    SDMReturnCode res = SDMReturnCode_Success;

    // External COM Port driver writes LPH2RR flag to the External COM Port TX.
    PSA_ADAC_ASSERT(EComSendFlag(FLAG_LPH2RR, "LPH1RR"), SDMReturnCode_Success);

    // External COM Port translates LPH2RR flag into a REMRR pulse signal which goes into the PMU.
    // LPH2RR flag is not inserted to the TX FIFO.

    // PMU generates a power on reset to the debugged system, including to the CPU and
    // the Internal COM Port device, the CryptoCell and its AON.
    // This power on reset does not reset the External COM port.

    // Return with success code
bail:
    return res;
}

SDMReturnCode ExternalComPortDriver::EComPort_Tx(uint8_t* TxBuffer, size_t TxBufferLength, size_t* actualLength, bool block)
{
    SDMReturnCode res = SDMReturnCode_Success;

    /* allocate new buffer that will include the extra bytes */
    size_t tempBuffLen = TxBufferLength * 2 + 2;
    uint8_t *tempBuffer = (uint8_t *) calloc(1, tempBuffLen);
    PSA_ADAC_ASSERT_ERROR(tempBuffer != NULL, true, SDMReturnCode_InternalError);

    PSA_ADAC_ASSERT_ERROR(mIsComPortInited == true, true, SDMReturnCode_RequestFailed);

    /* fill the array */
    PSA_ADAC_ASSERT(EComPortPrepareData(FLAG_START,
                                         TxBuffer,
                                         TxBufferLength,
                                         tempBuffer,
                                         tempBuffLen,
                                         actualLength),
                     SDMReturnCode_Success);

    PSA_ADAC_LOG_DUMP("  ----->  ", "data_to_send", tempBuffer, *actualLength);

    res = EComSendBlock(tempBuffer, *actualLength, block);
    if (res != SDMReturnCode_Success)
    {
        PSA_ADAC_LOG_ERR(ENTITY_NAME, "failed to send block data[%u]\n", (uint32_t)*actualLength);
        goto bail;
    }

    PSA_ADAC_LOG_DEBUG(ENTITY_NAME, "inSize[%zu] outSize[%zu]\n", TxBufferLength, *actualLength);

bail:
    free(tempBuffer);
    return res;
}

SDMReturnCode ExternalComPortDriver::EComPort_Rx(uint8_t* RxBuffer, size_t RxBufferLength, size_t* ActualLength)
{
    SDMReturnCode res = SDMReturnCode_Success;

    PSA_ADAC_ASSERT_ERROR(mIsComPortInited == true, true, SDMReturnCode_RequestFailed);

    PSA_ADAC_ASSERT(EComPortRxInt(FLAG_START, RxBuffer, RxBufferLength, ActualLength),
                     SDMReturnCode_Success);

bail:
    return res;
}

SDMReturnCode ExternalComPortDriver::EComPort_Finalize()
{
    // Disable the link:
    // 1.  transmits LPH2RL flag to the External COM port TX.

    // Internal COM Port activity upon link drop:
    //     1.  The Internal COM Port device driver reads the LPH2RL flag from the device
    //         FIFO and detects that the link is not established from the External COM Port.
    //     2.  The Internal COM Port device driver writes LPH2RL flag to the Internal COM Port device TX.
    //     3.  The Internal COM Port translates the LPH2RL flag into a latched LINKEST=0 signal.
    //         LPH2RL flag is not inserted to the TX FIFO.

    //     Wait for reverse link to fall:
    //     4.  The External COM Port detects that LINKEST signal is set to 0. As a result the
    //         HW inserts LPH2RL flag to the External COM Port’s RX FIFO.
    //     5.  The External COM Port driver polls its RX FIFO to detect LPH2RL flag.
    //         In case of timeout it skips the next step.
    //     6.  The debugger knows now that the link from the Internal COM Port to the External COM Port is dropped.

    SDMReturnCode res = SDMReturnCode_Success;

    PSA_ADAC_ASSERT(EComSendFlag(FLAG_LPH2RL, "LPH2RL"), SDMReturnCode_Success);
    PSA_ADAC_ASSERT(EComWaitFlag(FLAG_LPH2RL, "LPH2RL"), SDMReturnCode_Success);

    // 2.  EComPort_Power(PowerOff);
    PSA_ADAC_ASSERT(EComPort_Power(ECPD_POWER_OFF), SDMReturnCode_Success);
bail:
    return res;
}

SDMReturnCode ExternalComPortDriver::EComRxRaw(size_t numBytes, unsigned char* outBytes, size_t outBytesLength)
{
    if (numBytes == 0 || outBytesLength == 0 || outBytes == NULL)
    {
        return SDMReturnCode_InternalError;
    }
    
    // Do reads to APBCOM.DR
    // readSize = RxEngine width (FIDTXR.TXW) - only single byte implemented
    size_t readSize = 1;
    size_t drReads = (numBytes / readSize) + ((numBytes % readSize != 0) ? 1 : 0);
    size_t bytesRemaining = numBytes;

    // Create values and reg access op lists
    std::vector<uint32_t> drVals;
    std::vector<SDMRegisterAccess> accesses;
    try
    {
        drVals.assign(drReads, 0);
        accesses.assign(drReads, { 0, 0, 0, 0, 0 });
    }
    catch(const std::bad_alloc&)
    {
        return SDMReturnCode_InternalError;
    }

    uint32_t regAddr = mComDeviceRegisterBase + REG_DR;
    for (unsigned int i = 0; i < drReads; i++)
    {
        accesses[i].address = regAddr;
        accesses[i].op = SDMRegisterAccessOp_Read;
        accesses[i].value = &drVals[i];
    }

    size_t accessesCompleted = 0;
    SDMReturnCode result = mRegisterAccessCallback(&mComDevice, SDMTransferSize_32, &accesses[0], drReads, &accessesCompleted, mRefcon);
    if (result != SDMReturnCode_Success)
    {
        return result;
    }

    if (accessesCompleted != drReads)
    {
        return SDMReturnCode_RequestFailed;
    }

    // copy values to outBytes and check for individual errors
    for (unsigned int i = 0; i < drReads; i++)
    {
        // check we are within outBytesLength before copying
        size_t outBytesFree = outBytesLength - (i * readSize);
        size_t copyBytes = std::min(std::min(readSize, outBytesFree), bytesRemaining);

        memcpy(outBytes + (i * readSize), &drVals[i], copyBytes);

        bytesRemaining -= readSize;
    }

    return result;
}

SDMReturnCode ExternalComPortDriver::EComTxRaw(bool block, size_t numBytes, const unsigned char* inData)
{
    if (numBytes == 0 || inData == NULL)
    {
        return SDMReturnCode_InternalError;
    }

    // Do writes to APBCOM.DR
    // writeSize = TxEngine width (FIDTXR.TXW) - only single byte implemented
    size_t writeSize = 1;
    size_t drWrites = (numBytes / writeSize) + ((numBytes % writeSize != 0) ? 1 : 0);
    size_t bytesRemaining = numBytes;

    // Create values and reg access op lists
    std::vector<uint32_t> drVals;
    std::vector<SDMRegisterAccess> accesses;
    try
    {
        drVals.assign(drWrites, 0xAFAFAFAF); // pre-fill values with null bytes (0xAF)
        accesses.assign(drWrites, {0, 0, 0, 0, 0});
    }
    catch(const std::bad_alloc&)
    {
        return SDMReturnCode_InternalError;
    }

    // set reg acc op values

    uint32_t regAddr =  mComDeviceRegisterBase + (block ? REG_DBR : REG_DR);
    for (unsigned int i = 0; i < drWrites; i++)
    {
        // assuming writes are little-endian - specification states only LSB is used
        memcpy(&drVals[i], inData + (i * writeSize), std::min(bytesRemaining, writeSize));

        accesses[i].address = regAddr;
        accesses[i].op = SDMRegisterAccessOp_Write;
        accesses[i].value = &drVals[i];

        bytesRemaining -= writeSize;
    }

    size_t accessesCompleted = 0;
    SDMReturnCode result = mRegisterAccessCallback(&mComDevice, SDMTransferSize_32, &accesses[0], drWrites, &accessesCompleted, mRefcon);
    if (accessesCompleted != drWrites)
    {
        return SDMReturnCode_RequestFailed;
    }

    return result;
}

SDMReturnCode ExternalComPortDriver::EComStatus(uint8_t* txFree, uint8_t* txOverflow, uint8_t* rxData, uint8_t* linkErrs)
{
    uint32_t srVal = 0;
    SDMRegisterAccess accesses[1] = {
        {
            (uint64_t)(mComDeviceRegisterBase + REG_SR), // address
            SDMRegisterAccessOp_Read,                    // op
            &srVal,                                      // value
            0x0,                                         // pollMask
            0                                            // retries
        }
    };
    size_t accessesCompleted = 0;

    SDMReturnCode result = mRegisterAccessCallback(&mComDevice, SDMTransferSize_32, accesses, 1, &accessesCompleted, mRefcon);
    if (result != SDMReturnCode_Success)
    {
        return result;
    }

    if (accessesCompleted != 1)
    {
        return SDMReturnCode_RequestFailed;
    }

    //Build out params from SR
    if (txFree != NULL)
    {
        *txFree = srVal & 0xFF;                     //SR[7:0] - TxEngine FIFO space
    }

    if (txOverflow != NULL)
    {
        *txOverflow = ((1 << 13) & srVal) ? 1 : 0;  //SR[13] - TxEngine overflow
    }

    if (rxData != NULL)
    {
        *rxData = (srVal >> 16) & 0xFF;             //SR[23:16] - RxEngine full level
    }

    if (linkErrs != NULL)
    {
        *linkErrs = 0;
        *linkErrs |= ((1 << 14) & srVal) ? 1 : 0;   //SR[14] - TxEngine link error detected
        *linkErrs |= ((1 << 30) & srVal) ? 2 : 0;   //SR[30] - RxEngine link error detected
    }

    return result;
}
