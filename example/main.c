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
#include "csapbcom.h"

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

#include "secure_debug_manager.h"
#include "sdc600_log.h"

#define ENTITY_NAME "DS5"


/******************************************************************************************************
 *
 * Structs
 *
 ******************************************************************************************************/

static CSAPBCOMConnectionDescription gConnDesc;
static SDMDebugIf gSdmDebugIf;

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
    fprintf(stderr, "\tRESET_TYPE :\n");
    fprintf(stderr, "\t\t-1 : No reset.\n");
    fprintf(stderr, "\t\t0  : SDC-600 COM port reset.\n");
    fprintf(stderr, "\t\t>0 : nSRST. Perform system reset via the DAP. Value should be the RDDI device index (index within SDF file) of the system DAP.\n");
    fprintf(stderr, "\tAP_INDEX : RDDI device index (index within SDF file) of the COM-AP or APBCOM device.\n");
}

void progIndication(uint32_t step, uint8_t percentComplete)
{

    char buf[20] = "";
    uint32_t i;

    for (i = 0; i < (uint32_t)percentComplete / 10; ++i)
    {
        sprintf(buf + strlen(buf), "*");
    }

    SDC600_LOG_INFO(buf, "stage [%" PRIu32 "] %" PRIu8 "%% complete\n", step, percentComplete);
}

uint8_t rstStage1(void* context)
{
    SDC600_LOG_INFO("reset", "f_nSRSTStage1\n");
    CSAPBCOMHandle* handle = (CSAPBCOMHandle*)context;
    if (handle == NULL)
    {
        SDC600_LOG_ERR(ENTITY_NAME, "invalid context parameter\n");
        return 1;
    }
    CSAPBCOMReturnCode ret = CSAPBCOM_SystemReset(*handle, CSAPBCOM_RESET_BEGIN);
    if (ret != CSAPBCOM_SUCCESS)
    {
        SDC600_LOG_ERR(ENTITY_NAME, "failed with error [0x%04x]\n", (unsigned)ret);
        return 1;
    }

    return 0;
}

uint8_t rstStage2(void* context)
{
    SDC600_LOG_INFO("reset", "f_nSRSTStage2\n");
    CSAPBCOMHandle* handle = (CSAPBCOMHandle*)context;
    if (handle == NULL)
    {
        SDC600_LOG_ERR(ENTITY_NAME, "invalid context parameter\n");
        return 1;
    }
    CSAPBCOMReturnCode ret = CSAPBCOM_SystemReset(*handle, CSAPBCOM_RESET_END);
    if (ret != CSAPBCOM_SUCCESS)
    {
        SDC600_LOG_ERR(ENTITY_NAME, "failed with error [0x%04x]\n", (unsigned)ret);
        return 1;
    }

    return 0;
}

bool debug_if_init(SDMDebugIf* debugIF,
                          const char* address,
                          const char* sdf,
                          int dapIndex,
                          int deviceIndex)
{
    gConnDesc.sdf = sdf;
    gConnDesc.address = address;
    gConnDesc.dapIndex = dapIndex;
    gConnDesc.deviceIndex = deviceIndex;

    debugIF->version = 1;

    debugIF->pTopologyDetails = &gConnDesc;

    debugIF->callbacks = (SDMCallbacks *)malloc(sizeof(SDMCallbacks));
    if (debugIF->callbacks == NULL)
    {
        SDC600_LOG_ERR(ENTITY_NAME, "failed to allocate necessary memory\n");
        return false;
    }

    debugIF->callbacks->f_progressIndicationCallbackFunc = progIndication;
    debugIF->callbacks->f_nSRSTStage1 = rstStage1;
    debugIF->callbacks->f_nSRSTStage2 = rstStage2;

    return true;
}


int main(int argc, char** argv)
{
    SDMReturnCode rc = SDM_SUCCESS;

    // Get the connection address, dap index and SDF file
    if (argc != 5)
    {
        PrintUsage(argv[0]);
        return EXIT_FAILURE;
    }

    const char* address = argv[1];
    const char* sdf = argv[2];
    int resetTypeArg = atoi(argv[3]);
    int apIndex = atoi(argv[4]);

    int result = EXIT_SUCCESS;

    // DAP index is only rquired for nSRST
    // CSAPBCOM interface -1 indicates no DAP index provided
    int dapIndex = -1;
    SDMResetType resetType;
    if (resetTypeArg < 0)
        resetType = SDM_none;
    else if (resetTypeArg == 0)
        resetType = SDM_COMPortReset;
    else
    {
        resetType = SDM_nSRSTReset;
        dapIndex = resetTypeArg;
    }

    /* Create the debug interface and set its callbacks */
    if (!debug_if_init(&gSdmDebugIf, address, sdf, dapIndex, apIndex))
    {
        return EXIT_FAILURE;
    }

    rc = SDM_Init(resetType, &gSdmDebugIf);
    if (rc == SDM_SUCCESS_WAIT_RESUME)
    {
        rc = SDM_ResumeBoot();
        if (rc != SDM_SUCCESS)
        {
            printf("Error: SDM_ResumeBoot failed with code: 0x%08x\n", rc);
            result = EXIT_FAILURE;
            goto disconnect;
        }
    }
    else if (rc != SDM_SUCCESS)
    {
        printf("Error: SDM_Init failed with code: 0x%08x\n", rc);
        result = EXIT_FAILURE;
        goto disconnect;
    }

    SDC600_LOG_INFO(ENTITY_NAME, "System is open for debug\n");

disconnect:
    rc = SDM_End(SDM_none);
    if (rc != SDM_SUCCESS)
    {
        printf("Error: SDM_End failed with code: 0x%08x\n", rc);
        result = EXIT_FAILURE;
    }

    return result;
}
