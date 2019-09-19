// csapbcom_interface.cpp
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#include "csapbcom.h"

#include "rddi.h"
#include "rddi_debug.h"

#include "rddiwrapper/rddi_wrapper_impl.h"

#include <map>
#include <stdint.h>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>

namespace
{
    IRDDIWrapper* rddiWrapper = NULL;

    std::map<CSAPBCOMHandle, RDDIHandle> handleMap;

    typedef struct DevInfo
    {
        int dapIndex;
        int deviceIndex;
        int ctrlStatOffset;
    } DevInfo;
    std::map<CSAPBCOMHandle, DevInfo> deviceInfoMap;

    CSAPBCOMHandle GenerateCSAPBCOMHandle(RDDIHandle rddiHandle)
    {
        // Knuth's multiplicative hash
        return static_cast<uint32>(rddiHandle) * 2654435761;
    }

    // This implementation's version details
    std::string VersionDetails = "CSAPBCOM RDDI V3.0";

    // RDDI DP.CSW address for system resets
    const uint32 DPCtrlStatAddr = 0x2081;
    const int MaxDAPPWRACKPolls = 100;

    // Control and Status Register offsets
    const uint16 DROffset = 0x20;
    const uint16 SROffset = 0x2C;
    const uint16 DBROffset = 0x30;

    // default register access op
    RDDI_REG_ACC_OP defaultAccOp = {
        0xffffffff,    //regId
        1,             //regSize 1 = 32bit
        1,             //rwFlag 1 = write
        NULL,          //pRegValue
        0,             //errorCode
        0,             //errorLenght
        NULL,          //pErrorMsg
    };

    bool WaitForACK(unsigned int mask, unsigned int value, unsigned int& ctrlStat,
            RDDIHandle rddiHandle, int dapDevice)
    {
        int attempts = 0;
        int result = RDDI_SUCCESS;
        while ( ((ctrlStat & mask) != value) &&
                (result == RDDI_SUCCESS) &&
                (attempts < MaxDAPPWRACKPolls))
        {
            result = rddiWrapper->Debug_RegReadBlock(rddiHandle, dapDevice, DPCtrlStatAddr, 1, &ctrlStat, 1);
            attempts++;
        }

        return (attempts < MaxDAPPWRACKPolls) && (result == RDDI_SUCCESS);
    }
}

void InjectRDDIWrapper(IRDDIWrapper* wrapper)
{
    rddiWrapper = wrapper;
}

CSAPBCOMReturnCode CSAPBCOM_GetInterfaceVersion(char* versionDetails,
        size_t versionDetailsLength)
{
    if (!versionDetails)
    {
        return CSAPBCOM_BADARG;
    }

    if (versionDetailsLength == 0)
    {
        return CSAPBCOM_BADARG;
    }

    strncpy(versionDetails, VersionDetails.c_str(), versionDetailsLength);

    // Always null terminate
    versionDetails[versionDetailsLength - 1] = '\0';

    if (VersionDetails.length() > versionDetailsLength - 1)
    {
        return CSAPBCOM_BUFFER_OVERFLOW;
    }

    return CSAPBCOM_SUCCESS;
}

CSAPBCOMReturnCode CSAPBCOM_Open(CSAPBCOMHandle* outHandle,
        const CSAPBCOMConnectionDescription* topology)
{
    if (!outHandle || !topology)
    {
        return CSAPBCOM_BADARG;
    }

    if (rddiWrapper == NULL)
    {
        try
        {
            rddiWrapper = new RDDINativeWrapperImplementation();
        }
        catch (const std::bad_alloc&)
        {
            return CSAPBCOM_OUT_OF_MEM;
        }
    }

    /*
     * First open RDDI to retrieve our handle
     */
    RDDIHandle rddiHandle = INV_HANDLE;
    int result = rddiWrapper->Open(&rddiHandle, NULL);
    if (result != RDDI_SUCCESS)
    {
        return static_cast<CSAPBCOMReturnCode>(result);
    }

    if (rddiHandle == INV_HANDLE)
    {
        return CSAPBCOM_INTERNAL_ERROR;
    }

    /*
     * Now load up the SDF file and prepeare to connect to the debug
     * vehicle.
     */
    result = rddiWrapper->ConfigInfo_OpenFileAndRetarget(rddiHandle, topology->sdf, topology->address);
    if (result != RDDI_SUCCESS)
    {
        rddiWrapper->Close(rddiHandle);
        return static_cast<CSAPBCOMReturnCode>(result);
    }

    /*
     * Finally, make the connection to the debug vehicle
     */
    const size_t clientInfoLength = 128, iceInfoLength = 128, copyrightInfoLength = 128;
    char clientInfo[clientInfoLength] = {0};
    char iceInfo[iceInfoLength] = {0};
    char copyrightInfo[copyrightInfoLength] = {0};

    result = rddiWrapper->Debug_Connect(rddiHandle,
            "CSAPBCOM_connection",
            clientInfo, clientInfoLength,
            iceInfo, iceInfoLength,
            copyrightInfo, copyrightInfoLength);
    if (result != RDDI_SUCCESS)
    {
        rddiWrapper->Close(rddiHandle);
        return static_cast<CSAPBCOMReturnCode>(result);
    }

    // Add the handle to the internal map
    CSAPBCOMHandle CSAPBCOMHandle = GenerateCSAPBCOMHandle(rddiHandle);
    handleMap.insert(std::make_pair(CSAPBCOMHandle, rddiHandle));

    // Store DAP and device index
    DevInfo devInfo;
    devInfo.dapIndex = topology->dapIndex;
    devInfo.deviceIndex = topology->deviceIndex;
    devInfo.ctrlStatOffset = -1;

    deviceInfoMap.insert(std::make_pair(CSAPBCOMHandle, devInfo));

    *outHandle = CSAPBCOMHandle;

    return CSAPBCOM_SUCCESS;
}

CSAPBCOMReturnCode CSAPBCOM_Close(CSAPBCOMHandle handle)
{
    if (rddiWrapper == NULL)
    {
        return CSAPBCOM_INTERNAL_ERROR;
    }

    if (handleMap.find(handle) == handleMap.end())
    {
        return CSAPBCOM_INVALID_HANDLE;
    }
    RDDIHandle rddiHandle = handleMap[handle];

    int result = rddiWrapper->Debug_Disconnect(rddiHandle, 0);
    if (result != RDDI_SUCCESS)
    {
        return static_cast<CSAPBCOMReturnCode>(result);
    }

    result = rddiWrapper->Close(rddiHandle);

    // Remove entries into the dap/handle map
    deviceInfoMap.erase(handle);
    handleMap.erase(handle);

    return static_cast<CSAPBCOMReturnCode>(result);
}

CSAPBCOMReturnCode CSAPBCOM_Connect(CSAPBCOMHandle handle)
{
    if (rddiWrapper == NULL)
    {
        return CSAPBCOM_INTERNAL_ERROR;
    }

    if (handleMap.find(handle) == handleMap.end())
    {
        return CSAPBCOM_INVALID_HANDLE;
    }

    RDDIHandle rddiHandle = handleMap[handle];
    int device = deviceInfoMap[handle].deviceIndex;

    const size_t messageLength = 128;
    int id = 0;
    int version = 0;
    char message[messageLength] = {0};

    int result = rddiWrapper->Debug_OpenConn(rddiHandle, device, &id, &version, message, messageLength);
    if (result != RDDI_SUCCESS)
    {
        return static_cast<CSAPBCOMReturnCode>(result);
    }

    // Check if device is COM-AP (SoC-400) or APBCOM (SoC-600)
    // AP templates return the contents of IDR, peripheral devices return pid
    // The Control and Status Register have a different offset depending on the varient
    if (id == 0x04762000)
    {
        deviceInfoMap[handle].ctrlStatOffset = 0x0;
    }
    else if (id == 0x9ef)
    {
        deviceInfoMap[handle].ctrlStatOffset = 0xD00;
    }
    else
    {
        rddiWrapper->Debug_CloseConn(rddiHandle, device);
        return CSAPBCOM_WRONG_DEV;
    }

    return CSAPBCOM_SUCCESS;
}

CSAPBCOMReturnCode CSAPBCOM_Disconnect(CSAPBCOMHandle handle)
{
    if (rddiWrapper == NULL)
    {
        return CSAPBCOM_INTERNAL_ERROR;
    }

    if (handleMap.find(handle) == handleMap.end())
    {
        return CSAPBCOM_INVALID_HANDLE;
    }
    RDDIHandle rddiHandle = handleMap[handle];
    int device = deviceInfoMap[handle].deviceIndex;

    int result = rddiWrapper->Debug_CloseConn(rddiHandle, device);
    return static_cast<CSAPBCOMReturnCode>(result);
}

CSAPBCOMReturnCode CSAPBCOM_ReadData(CSAPBCOMHandle handle,
        size_t numBytes, unsigned char* outBytes, size_t outBytesLength)
{
    if (rddiWrapper == NULL)
    {
        return CSAPBCOM_INTERNAL_ERROR;
    }

    if (handleMap.find(handle) == handleMap.end())
    {
        return CSAPBCOM_INVALID_HANDLE;
    }

    if (numBytes == 0 || outBytesLength == 0 || outBytes == NULL)
    {
        return CSAPBCOM_BADARG;
    }

    RDDIHandle rddiHandle = handleMap[handle];
    int device = deviceInfoMap[handle].deviceIndex;
    int ctrlStatOffset = deviceInfoMap[handle].ctrlStatOffset;
    int result = CSAPBCOM_SUCCESS;

    // Do reads to APBCOM.DR
    // readSize = RxEngine width (FIDTXR.TXW) - only single byte implemented
    size_t readSize = 1;
    size_t drReads = (numBytes / readSize) + ((numBytes % readSize != 0) ? 1 : 0);
    size_t bytesRemaining = numBytes;

    // Create values and reg access op lists
    std::vector<RDDI_REG_ACC_OP> accOpList;
    std::vector<uint32_t> drVals;
    try
    {
        accOpList.assign(drReads, defaultAccOp);
        drVals.assign(drReads, 0);
    }
    catch(const std::bad_alloc&)
    {
        return CSAPBCOM_OUT_OF_MEM;
    }

    // set reg acc op values
    for (unsigned int i = 0; i < drReads; i++)
    {
        accOpList[i].registerID = (ctrlStatOffset + DROffset) / 4;
        accOpList[i].pRegisterValue = &drVals[i];
        accOpList[i].rwFlag = 0; //read
    }

    result = rddiWrapper->Debug_RegRWList(rddiHandle, device, &accOpList[0], drReads);
    if (result != RDDI_SUCCESS)
    {
        return static_cast<CSAPBCOMReturnCode>(result);
    }

    // copy values to outBytes and check for individual errors
    for (unsigned int i = 0; i < drReads; i++)
    {
        // check we are within outBytesLength before copying
        size_t outBytesFree = outBytesLength - (i * readSize);
        size_t copyBytes = std::min(std::min(readSize, outBytesFree), bytesRemaining);

        memcpy(outBytes + (i * readSize), &drVals[i], copyBytes);

        bytesRemaining -= readSize;

        if (accOpList[i].errorCode != RDDI_SUCCESS)
        {
            result = accOpList[i].errorCode;
        }
    }

    return static_cast<CSAPBCOMReturnCode>(result);
}

CSAPBCOMReturnCode CSAPBCOM_WriteData(CSAPBCOMHandle handle,
        int block, size_t numBytes, const unsigned char* inData)
{
    if (rddiWrapper == NULL)
    {
        return CSAPBCOM_INTERNAL_ERROR;
    }

    if (handleMap.find(handle) == handleMap.end())
    {
        return CSAPBCOM_INVALID_HANDLE;
    }

    if (numBytes == 0 || inData == NULL)
    {
        return CSAPBCOM_BADARG;
    }

    RDDIHandle rddiHandle = handleMap[handle];
    int device = deviceInfoMap[handle].deviceIndex;
    int ctrlStatOffset = deviceInfoMap[handle].ctrlStatOffset;
    int result = CSAPBCOM_SUCCESS;

    // Do writes to APBCOM.DR
    // writeSize = TxEngine width (FIDTXR.TXW) - only single byte implemented
    size_t writeSize = 1;
    size_t drWrites = (numBytes / writeSize) + ((numBytes % writeSize != 0) ? 1 : 0);
    size_t bytesRemaining = numBytes;

    // Create values and reg access op lists
    std::vector<RDDI_REG_ACC_OP> accOpList;
    std::vector<uint32_t> drVals;
    try
    {
        accOpList.assign(drWrites, defaultAccOp);
        drVals.assign(drWrites, 0xAFAFAFAF); // pre-fill values with null bytes (0xAF)
    }
    catch(const std::bad_alloc&)
    {
        return CSAPBCOM_OUT_OF_MEM;
    }

    for (unsigned int i = 0; i < drWrites; i++)
    {
        // assuming writes are little-endian - specification states only LSB is used
        memcpy(&drVals[i], inData + (i * writeSize), std::min(bytesRemaining, writeSize));

        // set reg acc op value and regID
        accOpList[i].pRegisterValue = &drVals[i];

        accOpList[i].registerID = (ctrlStatOffset + (block ? DBROffset : DROffset)) / 4;

        bytesRemaining -= writeSize;
    }

    result = rddiWrapper->Debug_RegRWList(rddiHandle, device, &accOpList[0], drWrites);

    // check for individual write errors
    for (unsigned int i = 0; i < drWrites; i++)
    {
        if (accOpList[i].errorCode != RDDI_SUCCESS)
        {
            result = accOpList[i].errorCode;
        }
    }

    return static_cast<CSAPBCOMReturnCode>(result);
}

CSAPBCOMReturnCode CSAPBCOM_SystemReset(CSAPBCOMHandle handle, CSAPBCOMResetParams resetType)
{
    if (rddiWrapper == NULL)
    {
        return CSAPBCOM_INTERNAL_ERROR;
    }

    if (handleMap.find(handle) == handleMap.end())
    {
        return CSAPBCOM_INVALID_HANDLE;
    }
    RDDIHandle rddiHandle = handleMap[handle];

    // check we have a valid DAP index - optional
    // SystemReset not possible without
    int dap = deviceInfoMap[handle].dapIndex;
    if (dap < 1)
    {
        return CSAPBCOM_WRONG_DEV;
    }

    int result = CSAPBCOM_SUCCESS;

    // connect to DAP device
    const size_t messageLength = 128;
    int id = 0;
    int version = 0;
    char message[messageLength] = {0};

    result = rddiWrapper->Debug_OpenConn(rddiHandle, dap, &id, &version, message, messageLength);
    if (result != RDDI_SUCCESS)
    {
        return static_cast<CSAPBCOMReturnCode>(result);
    }

    if (resetType == CSAPBCOM_RESET_BEGIN)
    {
        uint32 ctrlStat = 0x00000000;

        // Read modify write the DAP CtrlStat bits
        result = rddiWrapper->Debug_RegReadBlock(rddiHandle, dap, DPCtrlStatAddr, 1, &ctrlStat, 1);
        if (result != RDDI_SUCCESS)
        {
            rddiWrapper->Debug_CloseConn(rddiHandle, dap);
            return static_cast<CSAPBCOMReturnCode>(result);
        }

        // Power down DBG & SYS
        ctrlStat &= ~0x50000000;
        result = rddiWrapper->Debug_RegWriteBlock(rddiHandle, dap, DPCtrlStatAddr, 1, &ctrlStat);
        if (result != RDDI_SUCCESS)
        {
            rddiWrapper->Debug_CloseConn(rddiHandle, dap);
            return static_cast<CSAPBCOMReturnCode>(result);
        }

        // ACK should go low
        if (!WaitForACK(0xA0000000, 0x0, ctrlStat, rddiHandle, dap))
        {
            rddiWrapper->Debug_CloseConn(rddiHandle, dap);
            return CSAPBCOM_FAILED;
        }

        // Drive nSRST low
        result = rddiWrapper->Debug_SystemReset(rddiHandle, 0, RDDI_RST_ASSERT);
        if (result != RDDI_SUCCESS)
        {
            rddiWrapper->Debug_CloseConn(rddiHandle, dap);
            return static_cast<CSAPBCOMReturnCode>(result);
        }

        // Power up DBG & SYS
        ctrlStat = 0x50000000;
        result = rddiWrapper->Debug_RegWriteBlock(rddiHandle, dap, DPCtrlStatAddr, 1, &ctrlStat);
        if (result != RDDI_SUCCESS)
        {
            rddiWrapper->Debug_CloseConn(rddiHandle, dap);
            return static_cast<CSAPBCOMReturnCode>(result);
        }

        // ACK should go high
        if (!WaitForACK(0xA0000000, 0xA0000000, ctrlStat, rddiHandle, dap))
        {
            rddiWrapper->Debug_CloseConn(rddiHandle, dap);
            return CSAPBCOM_FAILED;
        }
    }
    else
    {
        // Drive nSRST high
        result = rddiWrapper->Debug_SystemReset(rddiHandle, 0, RDDI_RST_DEASSERT);
        if (result != RDDI_SUCCESS)
        {
            rddiWrapper->Debug_CloseConn(rddiHandle, dap);
            return static_cast<CSAPBCOMReturnCode>(result);
        }
    }

    // disconnect DAP device
    result = rddiWrapper->Debug_CloseConn(rddiHandle, dap);
    return static_cast<CSAPBCOMReturnCode>(result);
}

CSAPBCOMReturnCode CSAPBCOM_GetStatus(CSAPBCOMHandle handle, uint8_t* txFree, uint8_t* txOverflow,
        uint8_t* rxData, uint8_t* linkErrs)
{
    if (rddiWrapper == NULL)
    {
        return CSAPBCOM_INTERNAL_ERROR;
    }

    if (handleMap.find(handle) == handleMap.end())
    {
        return CSAPBCOM_INVALID_HANDLE;
    }

    RDDIHandle rddiHandle = handleMap[handle];
    int device = deviceInfoMap[handle].deviceIndex;
    int ctrlStatOffset = deviceInfoMap[handle].ctrlStatOffset;
    int result = CSAPBCOM_SUCCESS;

    // Read APBCOM.SR
    uint32 srVal = 0;
    result = rddiWrapper->Debug_RegReadBlock(rddiHandle, device, ((ctrlStatOffset + SROffset) /  4), 1, &srVal, 1);
    if (result != RDDI_SUCCESS)
    {
        return static_cast<CSAPBCOMReturnCode>(result);
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

    return CSAPBCOM_SUCCESS;
}
