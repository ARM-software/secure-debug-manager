// main.c
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

/**
 * \file
 *
 * \brief An example implementation showcasing CSAPBCOM library usage.
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include <vector>
#include <regex>

#include "secure_debug_manager.h"

#include "rddi_debug.h"
#include "rddi_configinfo.h"

#define BUFFER_LENGTH 128

static int gDAPIndex = 0;
static int gCOMPortDeviceIndex = 0;
static RDDIHandle gRDDIHandle = 0;

// RDDI DP.CSW ID for system resets
#define DP_CTRL_STAT_REG_ID 0x2081

/**
 * \brief Simply print the usage of this executable
 *
 * \param[in] binname The filename of this binary (usually set in argv[0])
 *
 */
void PrintUsage(const char* binname)
{
    fprintf(stderr, "Usage: %s \n", binname);
    fprintf(stderr, "\tDSTREAM_ADDRESS : Address of debug vehicle, prefixed with protocol (TCP:/USB:).\n");
    fprintf(stderr, "\tSDF_PATHFILE : Path to an SDF file describing the target system\n");
    fprintf(stderr, "\tDAP_INDEX : RDDI device index (index within SDF file) of the system DAP.\n");
    fprintf(stderr, "\tCOM_INDEX : RDDI device index (index within SDF file) of the SDC-600 COM-AP or APBCOM device.\n");
}

bool WaitForACK(unsigned int mask, unsigned int value, unsigned int ctrlStat)
{
    int attempts = 0;
    int result = RDDI_SUCCESS;
    const int MaxDAPPWRACKPolls = 100;

    while ( ((ctrlStat & mask) != value) &&
            (result == RDDI_SUCCESS) &&
            (attempts < MaxDAPPWRACKPolls))
    {
        result = Debug_RegReadBlock(gRDDIHandle, gDAPIndex, DP_CTRL_STAT_REG_ID, 1, &ctrlStat, 1);
        attempts++;
    }

    return (attempts < MaxDAPPWRACKPolls) && (result == RDDI_SUCCESS);
}

int systemResetStart()
{
    if (gDAPIndex < 1)
    {
        return RDDI_WRONGDEV;
    }

    // Connect to DAP device
    int deviceId = 0;
    int version = 0;
    char message[BUFFER_LENGTH];

    int result = Debug_OpenConn(gRDDIHandle, gDAPIndex, &deviceId, &version, message, BUFFER_LENGTH);
    if (result != RDDI_SUCCESS)
    {
        return result;
    }

    uint32 ctrlStat = 0x00000000;

    // Read modify write the DAP CtrlStat bits
    result = Debug_RegReadBlock(gRDDIHandle, gDAPIndex, DP_CTRL_STAT_REG_ID, 1, &ctrlStat, 1);
    if (result != RDDI_SUCCESS)
    {
        Debug_CloseConn(gRDDIHandle, gDAPIndex);
        return result;
    }

    // Power down DBG & SYS
    ctrlStat &= ~0x50000000;
    result = Debug_RegWriteBlock(gRDDIHandle, gDAPIndex, DP_CTRL_STAT_REG_ID, 1, &ctrlStat);
    if (result != RDDI_SUCCESS)
    {
        Debug_CloseConn(gRDDIHandle, gDAPIndex);
        return result;
    }

    // ACK should go low
    if (!WaitForACK(0xA0000000, 0x0, ctrlStat))
    {
        Debug_CloseConn(gRDDIHandle, gDAPIndex);
        return RDDI_INTERNAL_ERROR;
    }

    // Drive nSRST low
    result = Debug_SystemReset(gRDDIHandle, 0, RDDI_RST_ASSERT);
    if (result != RDDI_SUCCESS)
    {
        Debug_CloseConn(gRDDIHandle, gDAPIndex);
        return result;
    }

    // Power up DBG & SYS
    ctrlStat = 0x50000000;
    result = Debug_RegWriteBlock(gRDDIHandle, gDAPIndex, DP_CTRL_STAT_REG_ID, 1, &ctrlStat);
    if (result != RDDI_SUCCESS)
    {
        Debug_CloseConn(gRDDIHandle, gDAPIndex);
        return result;
    }

    // ACK should go high
    if (!WaitForACK(0xA0000000, 0xA0000000, ctrlStat))
    {
        Debug_CloseConn(gRDDIHandle, gDAPIndex);
        return RDDI_INTERNAL_ERROR;
    }

    // Disconnect DAP device
    return Debug_CloseConn(gRDDIHandle, gDAPIndex);
}

int systemResetFinish()
{    
    return Debug_SystemReset(gRDDIHandle, 0, RDDI_RST_DEASSERT);
}

void updateProgress(const char *progressMessage, uint8_t percentComplete, void *refcon)
{
    printf("updateProgress: stage [%s] %" PRIu8 "%% complete\n", progressMessage, percentComplete);
}

void setErrorMessage(const char *errorMessage, const char *errorDetails, void *refcon)
{
    printf("setErrorMessage: errorMessage: %s, errorDetails: %s, refcon: %p\n", errorMessage, errorDetails, refcon);
}

SDMReturnCode resetStart(SDMResetType resetType, void* refcon)
{
    if (resetType != SDMResetType_Default && resetType != SDMResetType_Hardware)
    {
        // Only hardware reset is supported in this example
        printf("resetStart: unsupported reset type\n");
        return SDMReturnCode_UnsupportedOperation;
    }

    int rddiReturnCode = systemResetStart();
    if (rddiReturnCode != RDDI_SUCCESS)
    {
        printf("resetStart: failed with error [0x%04x]\n", (unsigned)rddiReturnCode);
        return SDMReturnCode_InternalError;
    }

    return SDMReturnCode_Success;
}

SDMReturnCode resetFinish(SDMResetType resetType, void* refcon)
{
    if (resetType != SDMResetType_Default && resetType != SDMResetType_Hardware)
    {
        // Only hardware reset is supported in this example
        printf("resetFinish: unsupported reset type\n");
        return SDMReturnCode_UnsupportedOperation;
    }
    
    int rddiReturnCode = systemResetFinish();
    if (rddiReturnCode != RDDI_SUCCESS)
    {
        printf("resetFinish: failed with error [0x%04x]\n", (unsigned)rddiReturnCode);
        return SDMReturnCode_InternalError;
    }

    return SDMReturnCode_Success;
}

SDMReturnCode readMemory(const SDMDeviceDescriptor *device, uint64_t address, SDMTransferSize transferSize, size_t transferCount, uint32_t attributes, void *data, void *refcon)
{
    printf("readMemory: unsupported callback\n");
    return SDMReturnCode_UnsupportedOperation;
}

SDMReturnCode writeMemory(const SDMDeviceDescriptor *device, uint64_t address, SDMTransferSize transferSize, size_t transferCount, uint32_t attributes, const void *value, void *refcon)
{
    printf("writeMemory: unsupported callback\n");
    return SDMReturnCode_UnsupportedOperation;
}

SDMReturnCode registerAccess(const SDMDeviceDescriptor *device, SDMTransferSize transferSize, const SDMRegisterAccess *accesses, size_t accessCount, size_t *accessesCompleted, void *refcon)
{
    if (device == 0 || accesses == 0 || accessesCompleted == 0)
    {
        return SDMReturnCode_InvalidArgument;
    }

    if (accessCount < 1)
    {
        return SDMReturnCode_Success;
    }

    for (int i = 0; i < accessCount; i++)
    {
        if (accesses[i].op == SDMRegisterAccessOp_Poll)
        {
            return SDMReturnCode_UnsupportedOperation;
        }
    }

    std::vector<RDDI_REG_ACC_OP> accessOperations;
    for (int i = 0; i < accessCount; i++)
    {
        RDDI_REG_ACC_OP accessOp;
        accessOp.registerID = accesses[i].address / 4;
        accessOp.registerSize = 1;
        accessOp.rwFlag = (accesses[i].op == SDMRegisterAccessOp_Read ? 0 : 1);
        accessOp.pRegisterValue = accesses[i].value;
        accessOp.errorCode = RDDI_SUCCESS;
        accessOp.errorLength = 0;
        accessOp.pErrorMsg = NULL;

        accessOperations.push_back(accessOp);
    }

    *accessesCompleted = 0;
    int result = Debug_RegRWList(gRDDIHandle, gCOMPortDeviceIndex, &accessOperations[0], accessCount);
    if (result != RDDI_SUCCESS)
    {
        printf("registerAccess : Debug_RegRWList fauled with error code %d\n", result);
        return SDMReturnCode_TransferError;
    }
    else
    {
        for (unsigned i = 0; i < accessCount; i++)
        {
            if (accessOperations[i].errorCode == RDDI_SUCCESS)
            {
                (*accessesCompleted)++;
            }
            else
            {
                printf("registerAccess : Debug_RegRWList failed for operation %d with error code %d: %s\n", i, accessOperations[i].errorCode, accessOperations[i].pErrorMsg);
            }
        }
    }
	
    return ((*accessesCompleted) == accessCount) ? SDMReturnCode_Success : SDMReturnCode_TransferError;
}

SDMReturnCode presentForm(const SDMForm *form, void *refcon)
{
    if (form == 0 || form->elements == 0)
    {
        return SDMReturnCode_InvalidArgument;
    }

    if (form->info)
    {
        printf("\nUser input: %s\n  %s\n\n", form->title, form->info);
    }
    else
    {
        printf("\nUser input: %s\n\n", form->title);
    }

    for (uint32_t i = 0; i < form->elementCount; i++)
    {
        if (form->elements[i] == 0)
        {
            return SDMReturnCode_InvalidArgument;
        }

        if (form->elements[i]->fieldType == SDMForm_PathSelect)
        {
            if (form->elements[i]->pathSelect.pathBuffer == 0 || form->elements[i]->pathSelect.pathBufferLength <= 0)
            {
                return SDMReturnCode_InvalidArgument;
            }

            printf("%s: \n", form->elements[i]->title);

            char userInput[FILENAME_MAX];
            if (fgets(userInput, FILENAME_MAX, stdin) == 0)
            {
                return SDMReturnCode_InternalError;
            }

            // Remove white spaces from beginning and end of string
            std::string userInputString(userInput);
            userInputString = std::regex_replace(userInputString, std::regex("\\s+$"), "");
            userInputString = std::regex_replace(userInputString, std::regex("^\\s+"), "");

            if (userInputString.size() + 1 > form->elements[i]->pathSelect.pathBufferLength)
            {
                printf("Provided file path exceeds max buffer size. Max size is %d characters.\n", FILENAME_MAX);
                return SDMReturnCode_InternalError;
            }
            strncpy(form->elements[i]->pathSelect.pathBuffer, userInputString.c_str(), userInputString.size() + 1);
        }
        else
        {
            printf("presentForm: Form element type %d is not supported in this example\n", form->elements[i]->fieldType);
        }
    }

    return SDMReturnCode_Success;
}

int RDDI_Initialize(const char* sdf, const char* address)
{
    int result = RDDI_Open(&gRDDIHandle, NULL);
    if (result != RDDI_SUCCESS)
    {
        return result;
    }


    result = ConfigInfo_OpenFileAndRetarget(gRDDIHandle, sdf, address);
    if (result != RDDI_SUCCESS)
    {
        RDDI_Close(gRDDIHandle);
        return result;
    }

    char clientInfo[BUFFER_LENGTH];
    char iceInfo[BUFFER_LENGTH];
    char copyrightInfo[BUFFER_LENGTH];

    result = Debug_Connect(gRDDIHandle,
            "SDM Example",
            clientInfo, BUFFER_LENGTH,
            iceInfo, BUFFER_LENGTH,
            copyrightInfo, BUFFER_LENGTH);

    if (result != RDDI_SUCCESS)
    {
        RDDI_Close(gRDDIHandle);
        return result;
    }

    return RDDI_SUCCESS;
}

int RDDI_Finalize()
{
    int disconResult = Debug_Disconnect(gRDDIHandle, 0);
    int closeResult = RDDI_Close(gRDDIHandle);

    return disconResult != RDDI_SUCCESS ? disconResult : closeResult;
}

int main(int argc, char** argv)
{
    // Get the connection address, DAP index and SDF file
    if (argc != 5)
    {
        PrintUsage(argv[0]);
        return EXIT_FAILURE;
    }

    const char* address = argv[1];
    const char* sdf = argv[2];
    gDAPIndex = atoi(argv[3]);
    gCOMPortDeviceIndex = atoi(argv[4]);

    int rddiRes = RDDI_Initialize(sdf, address);
    if(rddiRes != RDDI_SUCCESS)
    {
        printf("Error: RDDI_Initialize failed 0x%08x\n", rddiRes);
        return EXIT_FAILURE;
    }

    // Connect to the SDC-600 device
    int deviceId = 0;
    int version = 0;
    char message[BUFFER_LENGTH];

    rddiRes = Debug_OpenConn(gRDDIHandle, gCOMPortDeviceIndex, &deviceId, &version, message, BUFFER_LENGTH);
    if(rddiRes != RDDI_SUCCESS)
    {
        printf("Error: Debug_OpenConn failed 0x%08x\n", rddiRes);
        RDDI_Finalize();
        return EXIT_FAILURE;
    }

    SDMOpenParameters sdmOpenParams;

    // Check if SDC-600 device is COM-AP (SoC-400) or APBCOM (SoC-600)
    // AP templates return the contents of IDR, peripheral devices return pid
    // The Control and Status Register have a different offset depending on the varient
    static const uint32_t COMAP_IDR = 0x04762000;
    static const uint32_t APMCOM_PID = 0x9ef;
    if (deviceId == COMAP_IDR)
    {
        sdmOpenParams.debugArchitecture = SDMDebugArchitecture_ArmADIv5;
    }
    else if (deviceId == APMCOM_PID)
    {
        sdmOpenParams.debugArchitecture = SDMDebugArchitecture_ArmADIv6;
    }
    else
    {
        printf("Error: invalid SDC-600 device ID\n");
        Debug_CloseConn(gRDDIHandle, gCOMPortDeviceIndex);
        RDDI_Finalize();
        return EXIT_FAILURE;
    }

    sdmOpenParams.version.major = SDMVersion_CurrentMajor;
    sdmOpenParams.version.minor = SDMVersion_CurrentMinor;

    sdmOpenParams.refcon = 0;

    SDMCallbacks sdmCallbacks;
    sdmOpenParams.callbacks = &sdmCallbacks;
    sdmOpenParams.callbacks->architectureCallbacks = 0;
    sdmOpenParams.callbacks->updateProgress = updateProgress;
    sdmOpenParams.callbacks->setErrorMessage = setErrorMessage;
    sdmOpenParams.callbacks->resetStart = resetStart;
    sdmOpenParams.callbacks->resetFinish = resetFinish;
    sdmOpenParams.callbacks->readMemory = readMemory;
    sdmOpenParams.callbacks->writeMemory = writeMemory;
    sdmOpenParams.callbacks->registerAccess = registerAccess;
    sdmOpenParams.callbacks->presentForm = presentForm;

    SDMHandle sdmHandle;
    SDMReturnCode sdmRes = SDMOpen(&sdmHandle, &sdmOpenParams);
    if(sdmRes != SDMReturnCode_Success)
    {
        printf("Error: SDM_Open failed with code: 0x%08x\n", sdmRes);
        Debug_CloseConn(gRDDIHandle, gCOMPortDeviceIndex);
        RDDI_Finalize();
        return EXIT_FAILURE;
    }

    sdmRes = SDMAuthenticate(sdmHandle, NULL);
    if (sdmRes != SDMReturnCode_Success)
    {
        printf("Error: SDM_Authenticate failed with code: 0x%08x\n", sdmRes);
        SDMClose(sdmHandle);
        Debug_CloseConn(gRDDIHandle, gCOMPortDeviceIndex);
        RDDI_Finalize();
        return EXIT_FAILURE;
    }

    sdmRes = SDMResumeBoot(sdmHandle);
    if (sdmRes != SDMReturnCode_Success)
    {
        printf("Error: SDM_ResumeBoot failed with code: 0x%08x\n", sdmRes);
        SDMClose(sdmHandle);
        Debug_CloseConn(gRDDIHandle, gCOMPortDeviceIndex);
        RDDI_Finalize();
        return EXIT_FAILURE;
    }

    printf("System is open for debug\n");

    sdmRes = SDMClose(sdmHandle);
    if (sdmRes != SDMReturnCode_Success)
    {
        printf("Error: SDM_Close failed with code: 0x%08x\n", sdmRes);
    }

    Debug_CloseConn(gRDDIHandle, gCOMPortDeviceIndex);
    RDDI_Finalize();

    return EXIT_SUCCESS;
}
