// ext_com_port_driver.cpp
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/time.h>
#endif

#include "sdc600_log.h"

#include "csapbcom.h"

#include "ext_com_port_driver.h"

#define ENTITY_NAME "ECPD"

/******************************************************************************************************
 *
 * Build options
 *
 ******************************************************************************************************/
#define COM_PORT_HW_TX_BLOCKING true

/******************************************************************************************************
 *
 * Externs and variables
 *
 ******************************************************************************************************/
static CSAPBCOMHandle gHandle = 0;
static bool gIsComPortInited = false;
static ECPDRequiredState powerState = ECPD_POWER_OFF;

/******************************************************************************************************
 *
 * static declarations
 *
 ******************************************************************************************************/
static ECPDReturnCode EComPortRxInt(uint8_t startFlag,
                                  uint8_t* rxBuffer,
                                  size_t rxBufferLength,
                                  size_t* actualLength);

static ECPDReturnCode EComPortPrepareData(uint8_t startFlag,
                                     const uint8_t* data,
                                     size_t inSize,
                                     uint8_t* outData,
                                     size_t outDataBufferSize,
                                     size_t* outSize);

static ECPDReturnCode EComSendByte(uint8_t byte);
static ECPDReturnCode EComSendBlock(uint8_t* byteData, size_t dataLen);
static ECPDReturnCode EComReadByte(uint8_t* byte, bool* isTimeout);
static ECPDReturnCode EComSendFlag(uint8_t flag, const char* flag_name);
static ECPDReturnCode EComWaitFlag(uint8_t flag, bool* isTimeout, const char* flag_name);

/******************************************************************************************************
 *
 * private
 *
 ******************************************************************************************************/
static const char* apbcomflagToStr(uint8_t flag)
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

static ECPDReturnCode EComSendByte(uint8_t byte)
{
    ECPDReturnCode res = ECPD_SUCCESS;
    CSAPBCOMReturnCode result = CSAPBCOM_SUCCESS;

    uint8_t txFree = 0;
    uint8_t txOverflow = 0;
    uint8_t linkErrs = 0;

    const uint32_t MAX_NUM_OF_RETRIES = 5000;
    uint32_t numOfRetries = 0;

    uint8_t txData[] = { byte };

    /* wait for TXS byte value to indicate that the TX FIFO is not full */
    do
    {
        result = CSAPBCOM_GetStatus(gHandle, &txFree, &txOverflow, NULL, &linkErrs);
        if (result != CSAPBCOM_SUCCESS)
        {
            SDC600_LOG_ERR(ENTITY_NAME, "CSAPBCOM_GetStatus failed with code: 0x%x\n", result);
            res = ECPD_LINK_ERR;
            goto bail;
        }

        if (linkErrs != 0)
        {
            SDC600_LOG_ERR(ENTITY_NAME, "CSAPBCOM_GetStatus linkErrs[0x%08x]\n", linkErrs);
            res = ECPD_LINK_ERR;
            goto bail;
        }

        if (txOverflow != 0)
        {
            SDC600_LOG_ERR(ENTITY_NAME, "CSAPBCOM_GetStatus txOverflow[0x%08x]\n", txOverflow);
            res = ECPD_LINK_ERR;
            goto bail;
        }
    }
    while (txFree == 0 && numOfRetries++  < MAX_NUM_OF_RETRIES);

    if (numOfRetries == MAX_NUM_OF_RETRIES)
    {
        res = ECPD_TIMEOUT;
        goto bail;
    }

    // write byte to TX
    result = CSAPBCOM_WriteData(gHandle, 0, 1, txData);
    if (result != CSAPBCOM_SUCCESS)
    {
        SDC600_LOG_ERR(ENTITY_NAME, "CSAPBCOM_WriteData failed with code: 0x%x\n", result);
        res = ECPD_TX_FAIL;
        goto bail;
    }

bail:
    return res;
}

static ECPDReturnCode EComSendBlock(uint8_t* byteData, size_t dataLen)
{
    ECPDReturnCode res = ECPD_SUCCESS;
#if COM_PORT_HW_TX_BLOCKING == true
    CSAPBCOMReturnCode result = CSAPBCOM_WriteData(gHandle, 1, dataLen, byteData);
    if (result != CSAPBCOM_SUCCESS)
    {
        SDC600_LOG_ERR(ENTITY_NAME, "CSAPBCOM_WriteData failed with code: 0x%x\n", result);
        res = ECPD_TX_FAIL;
        goto bail;
    }
#else
    for (int i = 0; i < dataLen; i++)
    {
        res = EComSendByte(byteData[i]);
        if (res != ECPD_SUCCESS)
       {
           SDC600_LOG_ERR(ENTITY_NAME, "EComSendByte failed with code: 0x%x\n", res);
           res = ECPD_TX_FAIL;
           goto bail;
       }
    }
#endif

bail:
    return res;
}

static ECPDReturnCode EComReadByte(uint8_t* byte, bool* isTimeout)
{
    ECPDReturnCode res = ECPD_SUCCESS;
    CSAPBCOMReturnCode result = CSAPBCOM_SUCCESS;

    uint8_t txOverflow = 0;
    uint8_t rxStatusData = 0;
    uint8_t linkErrs = 0;

    const uint32_t MAX_ATTEMPTS = 5000;
    uint32_t attempt = 0;

    uint8_t rxData = 0x0;

    do
    {
        result = CSAPBCOM_GetStatus(gHandle, NULL, &txOverflow, &rxStatusData, &linkErrs);
        if (result != CSAPBCOM_SUCCESS)
        {
            SDC600_LOG_ERR(ENTITY_NAME, "CSAPBCOM_GetStatus failed with code: 0x%x\n", result);
            res = ECPD_LINK_ERR;
            goto bail;
        }

        if (linkErrs != 0)
        {
            SDC600_LOG_ERR(ENTITY_NAME, "CSAPBCOM_GetStatus linkErrs[0x%08x]\n", linkErrs);
            res = ECPD_LINK_ERR;
            goto bail;
        }

        if (txOverflow != 0)
        {
            SDC600_LOG_ERR(ENTITY_NAME, "CSAPBCOM_GetStatus txOverflow[0x%08x]\n", txOverflow);
            res = ECPD_LINK_ERR;
            goto bail;
        }
    }
    while (rxStatusData == 0 && attempt++ < MAX_ATTEMPTS);

    *isTimeout = false;
    if (attempt >= MAX_ATTEMPTS)
    {
        *isTimeout = true;
        res = ECPD_TIMEOUT;
        goto bail;
    }

    result = CSAPBCOM_ReadData(gHandle, sizeof(rxData), &rxData, sizeof(rxData));
    if (result != CSAPBCOM_SUCCESS)
    {
        SDC600_LOG_ERR(ENTITY_NAME, "CSAPBCOM_ReadData failed with code: 0x%x\n", result);
        res = ECPD_RX_FAIL;
        goto bail;
    }

    *byte = rxData;

bail:
    return res;
}

static ECPDReturnCode EComSendFlag(uint8_t flag, const char* flag_name)
{
    SDC600_LOG_INFO("--------->", "%s\n", flag_name);

    return EComSendByte(flag);
}

static ECPDReturnCode EComWaitFlag(uint8_t flag, bool* isTimeout, const char* flag_name)
{
    ECPDReturnCode res = ECPD_SUCCESS;
    uint8_t byte = FLAG__NULL;

    SDC600_LOG_DEBUG(ENTITY_NAME, "waiting for flag[%s]\n", apbcomflagToStr(flag));

    do
    {
        res = EComReadByte(&byte, isTimeout);
        if (res != ECPD_SUCCESS)
            break;
    }
    while (byte != flag);

    SDC600_LOG_INFO("<---------", "%s\n", flag_name);

    return res;
}


static ECPDReturnCode EComPortPrepareData(uint8_t startFlag, const uint8_t* data, size_t inSize, uint8_t* outData, size_t outDataBufferSize, size_t* outSize)
{
    ECPDReturnCode res = ECPD_SUCCESS;
    uint8_t *tempData = (uint8_t*) calloc(1, (inSize * 2) + 2);
    uint16_t outByteIndex = 0;
    uint32_t byte_index = 0;

    SDC600_ASSERT_ERROR(tempData != NULL, true, ECPD_TX_FAIL);

    tempData[outByteIndex++] = FLAG_START;

    for (byte_index = 0; byte_index < inSize; ++byte_index)
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
        SDC600_LOG_ERR(ENTITY_NAME,
                    "tempData buffer is too small, outByteIndex[%u] outDataBufferSize[%zu]\n",
                    outByteIndex,
                    outDataBufferSize);
        res = ECPD_BUFFER_OVERFLOW;
        goto bail;
    }
    else
    {
        memcpy(outData, tempData, outByteIndex);
        *outSize = outByteIndex;
    }

bail:
    free(tempData);

    return res;
}

static ECPDReturnCode EComPortRxInt(uint8_t startFlag, uint8_t* rxBuffer, size_t rxBufferLength, size_t* actualLength)
{
    ECPDReturnCode res = ECPD_SUCCESS;
    CSAPBCOMReturnCode result = CSAPBCOM_SUCCESS;

    const uint32_t MAX_PRE_NULL_FLAGS = 10000;
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
        result = CSAPBCOM_ReadData(gHandle, sizeof(uint8_t), &read_byte, sizeof(uint8_t));
        if (result != CSAPBCOM_SUCCESS)
        {
            SDC600_LOG_ERR(ENTITY_NAME, "CSAPBCOM_ReadData failed with code: 0x%x\n", result);
            res = ECPD_RX_FAIL;
            goto bail;
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
                return ECPD_TIMEOUT;

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
                SDC600_LOG_ERR(ENTITY_NAME, "RxBufferLength[%u] buffer_idx[%u]\n", (uint32_t)rxBufferLength, buffer_idx);
                res = ECPD_BUFFER_OVERFLOW;
                goto bail;
            }

            rxBuffer[buffer_idx] = read_byte;

            buffer_idx += 1;
            *actualLength = buffer_idx;
        }
    }

    SDC600_LOG_BUF("  <-----  ", rxBuffer, *actualLength, "data_recv");

bail:

    if (res == ECPD_SUCCESS)
    {
        if (isStartRecv ^ isEndRecv)
            res = ECPD_UNEXPECTED_SYMBOL;
    }

    return res;
}

/******************************************************************************************************
 *
 * public
 *
 ******************************************************************************************************/
ECPDReturnCode EComPort_Init(SDMResetType ResetType,
                       uint8_t* IDResponseBuffer,
                       size_t IDBufferLength,
                       SDMDebugIf* pDebugIF)
{
    ECPDReturnCode res = ECPD_SUCCESS;
    size_t actualLength = 0;
    bool isTimeout = false;

    static char interfaceVersionDetails[128];
    CSAPBCOM_GetInterfaceVersion(interfaceVersionDetails, sizeof(interfaceVersionDetails));

    if (strcmp(interfaceVersionDetails, "CSAPBCOM RDDI V3.0") != 0)
    {
        printf("Error: Unknown CSAPBCOM implementation. Reported implementation: %s\n",
            interfaceVersionDetails);

        return ECPD_NO_INTI;
    }

    gHandle = *((CSAPBCOMHandle*)pDebugIF->pTopologyDetails);

    // 1.  If ResetType is nSRSTReset
    // Call pDebugIF->f_nSRSTStage1
    // In case of bad status, return with an error.
    if (ResetType == SDM_nSRSTReset)
    {
        if (pDebugIF->callbacks->f_nSRSTStage1 != NULL)
        {
            SDC600_ASSERT_ERROR(pDebugIF->callbacks->f_nSRSTStage1(&gHandle), 0, ECPD_NO_INTI);
        }
        else
        {
            SDC600_ASSERT_ERROR(CSAPBCOM_SystemReset(gHandle, CSAPBCOM_RESET_BEGIN), CSAPBCOM_SUCCESS, ECPD_NO_INTI);
        }
    }

    // Setup the Internal COM Port’s power
    // 2.  External COM Port driver calls EComPort_Power(PowerOn)
    // In case of bad status, return with an error.
    SDC600_ASSERT(EComPort_Power(ECPD_POWER_ON), ECPD_SUCCESS);

    // Establish the link:
    // 3.  Transmits LPH2RA flag to the External COM port TX. External COM port HW will set the LINKEST signal to the Internal COM Port and drop the flag.
    // In case of bad status, return with an error.
    SDC600_ASSERT(EComSendFlag(FLAG_LPH2RA, "LPH2RA"), ECPD_SUCCESS);

    // 4. If ResetType is COMPortReset
    // Call EComPort_RReboot
    // In case of bad status, return with an error.
    // if ResetType is nSRSTReset
    // Call pDebugIF->f_nSRSTStage2
    // In case of bad status, return with an error.
    if (ResetType == SDM_COMPortReset)
    {
        SDC600_ASSERT(EComPort_RReboot(), ECPD_SUCCESS);
    }
    else if (ResetType == SDM_nSRSTReset)
    {
        if (pDebugIF->callbacks->f_nSRSTStage2 != NULL)
        {
            SDC600_ASSERT_ERROR(pDebugIF->callbacks->f_nSRSTStage2(&gHandle), 0, ECPD_NO_INTI);
        }
        else
        {
            SDC600_ASSERT_ERROR(CSAPBCOM_SystemReset(gHandle, CSAPBCOM_RESET_END), CSAPBCOM_SUCCESS, ECPD_NO_INTI);
        }
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
    SDC600_ASSERT(EComWaitFlag(FLAG_LPH2RA, &isTimeout, "LPH2RA"), ECPD_SUCCESS);
    SDC600_ASSERT_ERROR(isTimeout == false, true, ECPD_TIMEOUT);

    // 11. The External COM Port driver polls its RX FIFO to detect LPH2RA flag.
    //     In case of timeout it returns with link establishment timeout expired error.
    // 12. The debugger knows now that the link from the Internal COM Port to the External COM Port is established.
    //
    // External COM Port driver checks the debugged system protocol:
    // 13. The debugger transmits now an IDR flag - Identification Request (note: this is a single flag message with no START or END).
    SDC600_ASSERT(EComSendFlag(FLAG_IDR, "IDR"), ECPD_SUCCESS);

    // 14. The debugged system IComPortInit() function responds and transmits to the debugger with Identification response message. Note: this response message format has a special format. It starts with IDA flag, followed by 6 bytes of debugged system ID hex value, and an END flag. If any of the platform ID bytes has MS bits value of 101b then the transmit driver must send an ESC flag following a flip of the MS bit of the byte to transmit.
    SDC600_ASSERT(EComPortRxInt(FLAG_IDA, IDResponseBuffer, IDBufferLength, &actualLength), ECPD_SUCCESS);
    SDC600_ASSERT_ERROR(actualLength == 0, false, ECPD_RX_FAIL);
    SDC600_LOG_BUF("<---------", IDResponseBuffer, actualLength, "IDResponseBuffer");

    // 15. At this point the debugged system () API returns with success code.
    // 16. The debugger EComPort_Init() API saves the received platform ID (6IComPortInit bytes) in the provided buffer and returns with success code.

    gIsComPortInited = true;

bail:
    return res;
}

ECPDReturnCode EComPort_Power(ECPDRequiredState RequiredState)
{
    ECPDReturnCode res = ECPD_SUCCESS;
    bool isTimeout = false;

    if (RequiredState == ECPD_POWER_ON)
    {
        // Release link first to get it into a know state
        // write LPH1RL to TX
        SDC600_ASSERT(EComSendFlag(FLAG_LPH1RL, "LPH1RL"), ECPD_SUCCESS);

        // poll for LPH1RL to in RX
        SDC600_ASSERT(EComWaitFlag(FLAG_LPH1RL, &isTimeout, "LPH1RL"), ECPD_SUCCESS);
        SDC600_ASSERT_ERROR(isTimeout == false, true, ECPD_TIMEOUT);

        // write LPH1RA to TX
        SDC600_ASSERT(EComSendFlag(FLAG_LPH1RA, "LPH1RA"), ECPD_SUCCESS);

        // poll for LPH1RA to in RX
        SDC600_ASSERT(EComWaitFlag(FLAG_LPH1RA, &isTimeout, "LPH1RA"), ECPD_SUCCESS);
        SDC600_ASSERT_ERROR(isTimeout == false, true, ECPD_TIMEOUT);

    }

    if (RequiredState == ECPD_POWER_OFF)
    {
        SDC600_ASSERT(EComSendFlag(FLAG_LPH2RL, "LPH2RL"), ECPD_SUCCESS);

        // poll for LPH2RL to in RX
        SDC600_ASSERT(EComWaitFlag(FLAG_LPH2RL, &isTimeout, "LPH2RL"), ECPD_SUCCESS);
        SDC600_ASSERT_ERROR(isTimeout == false, true, ECPD_TIMEOUT);

        // Release link first to get it into a know state
        // write LPH1RL to TX
        SDC600_ASSERT(EComSendFlag(FLAG_LPH1RL, "LPH1RL"), ECPD_SUCCESS);

        // poll for LPH1RL to in RX
        SDC600_ASSERT(EComWaitFlag(FLAG_LPH1RL, &isTimeout, "LPH1RL"), ECPD_SUCCESS);
        SDC600_ASSERT_ERROR(isTimeout == false, true, ECPD_TIMEOUT);

    }

    powerState = RequiredState;

bail:

    return res;
}

ECPDReturnCode EComPort_RReboot(void)
{
    ECPDReturnCode res = ECPD_SUCCESS;

    // External COM Port driver writes LPH2RR flag to the External COM Port TX.
    SDC600_ASSERT(EComSendFlag(FLAG_LPH2RR, "LPH1RR"), ECPD_SUCCESS);

    // External COM Port translates LPH2RR flag into a REMRR pulse signal which goes into the PMU.
    // LPH2RR flag is not inserted to the TX FIFO.

    // PMU generates a power on reset to the debugged system, including to the CPU and
    // the Internal COM Port device, the CryptoCell and its AON.
    // This power on reset does not reset the External COM port.

    // Return with success code
bail:
    return res;
}

ECPDReturnCode EComPort_Tx(uint8_t* TxBuffer, size_t TxBufferLength, size_t* actualLength)
{
    ECPDReturnCode res = ECPD_SUCCESS;

    /* allocate new buffer that will include the extra bytes */
    size_t tempBuffLen = TxBufferLength * 2 + 2;
    uint8_t *tempBuffer = (uint8_t *) calloc(1, tempBuffLen);
    SDC600_ASSERT_ERROR(tempBuffer != NULL, true, ECPD_TX_FAIL);

    SDC600_ASSERT_ERROR(gIsComPortInited == true, true, ECPD_NO_INTI);

    /* fill the array */
    SDC600_ASSERT(EComPortPrepareData(FLAG_START,
                                         TxBuffer,
                                         TxBufferLength,
                                         tempBuffer,
                                         tempBuffLen,
                                         actualLength),
                     ECPD_SUCCESS);

    SDC600_LOG_BUF("  ----->  ", tempBuffer, *actualLength, "data_to_send");

    res = EComSendBlock(tempBuffer, *actualLength);
    if (res != ECPD_SUCCESS)
    {
        SDC600_LOG_ERR(ENTITY_NAME, "failed to send block data[%u]\n", (uint32_t)*actualLength);
        goto bail;
    }

    SDC600_LOG_DEBUG(ENTITY_NAME, "inSize[%zu] outSize[%zu]\n",
                  TxBufferLength,
                  *actualLength);

bail:
    free(tempBuffer);

    return res;
}

ECPDReturnCode EComPort_Rx(uint8_t* RxBuffer, size_t RxBufferLength, size_t* ActualLength)
{
    ECPDReturnCode res = ECPD_SUCCESS;

    SDC600_ASSERT_ERROR(gIsComPortInited == true, true, ECPD_NO_INTI);

    SDC600_ASSERT(EComPortRxInt(FLAG_START, RxBuffer, RxBufferLength, ActualLength),
                     ECPD_SUCCESS);

bail:
    return res;
}
