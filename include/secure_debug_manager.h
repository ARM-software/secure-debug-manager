// secure_debug_manager.h
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

 /**
 * \file
 *
 * \brief This header file is the entry point to Secure Debug Manager
 * functionality and includes all necessary definitions.
 */

#ifndef DS5_SECURE_DEBUG_MANAGER_H_
#define DS5_SECURE_DEBUG_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#ifdef SDM_EXPORT_SYMBOLS
#define SDM_EXTERN __declspec(dllexport)
#else
#define SDM_EXTERN __declspec(dllimport)
#endif
#else
#define SDM_EXTERN
#endif

#include "stdint.h"
#include "stddef.h"

/**
* \brief Reset method to perform during {@link SDM_Init} and {@link SDM_End}
*/
SDM_EXTERN typedef enum SDMResetType {
    SDM_none = 0, /*!< No reset*/
    SDM_COMPortReset = 1, /*!< SDC-600 External COM port remote reboot */
    SDM_nSRSTReset = 2 /*!< Full system reset via nSRST pin */
} SDMResetType;

/**
* \brief SDM_Init() progress steps reported via f_progressIndicationCallbackFunc callback
*/
SDM_EXTERN typedef enum SDMInitStep {
    SDM_Reserved = 0, /*!< Unused */
    SDM_COM_Port_Init_Done = 1, /*!< SDC-600 COM port driver and COM port link initialized */
    SDM_Received_Expected_IDA_Response = 2, /*!< IDA response from debugged system has been verified */
    SDM_Sent_Get_SOC_ID = 3, /*!< 'Get SoC Id' command sent */
    SDM_Received_SOC_ID = 4, /*!< 'Get SoC Id' response received */
    SDM_Creating_Secure_Debug_Certificate = 5, /*!< Secure debug certificate requested from Authentication Token Provider */
    SDM_Received_Secure_Debug_Certificate = 6, /*!< Secure debug certificate received from Authentication Token Provider*/
    SDM_Sent_Secure_Debug_Certificate = 7, /*!< 'Introduce Debug Certificate' command sent */
    SDM_Complete = 8 /*!< 'Introduce Debug Certificate' response received */
} SDMInitStep;

/**
* \brief Return codes from SDM_* functionality.
*/
SDM_EXTERN typedef enum SDMReturnCode {
    SDM_SUCCESS, /*!< Success, no error */
    SDM_SUCCESS_WAIT_RESUME, /*!< Success, debugged system waits for 'Resume Boot' command. {@link SDM_ResumeBoot} should follow */
    SDM_FAIL_NO_RESPONSE, /*!< No response, timeout */
    SDM_FAIL_UNEXPECTED_SYMBOL, /*!< Unexpected symbol received */
    SDM_FAIL_UNSUPPORTED_PROTOCOL_ID, /*!< Unsupported remote platform ID */
    SDM_FAIL_USER_CRED, /*!< Invalid user credentials for the debugged platform */
    SDM_FAIL_IO, /*!< Failed to transmit/recive data to/from the SDC-600 COM port device */
    SDM_FAIL_INTERNAL /*!< An unspecified internal error occurred */
} SDMReturnCode;

/**
* \brief Callback function for SDM_Init() to report SDMInitStep progress steps
*/
SDM_EXTERN typedef void(*f_progressIndicationCallback)(uint32_t step, uint8_t percentComplete);

/**
* \brief Collection of callback functions for SDM_Init() and EComPort_Init()
*/
SDM_EXTERN typedef struct SDMCallbaks {
    f_progressIndicationCallback f_progressIndicationCallbackFunc; /*!<  Progress report callback*/
    uint8_t(*f_nSRSTStage1)(); /*!<  nSRST stage 1 callback  */
    uint8_t(*f_nSRSTStage2)(); /*!<  nSRST stage 2 callback  */
} SDMCallbaks;

/**
* \brief Collection of connection details for SDM_Init() and EComPort_Init()
*/
SDM_EXTERN typedef struct SDMDebugIf {
    uint32_t version; /*!< Client interface version  */
    void* pTopologyDetails; /*!< Topology/connection details for I/O driver and debug vehicle */
    SDMCallbaks *callbacks; /*!< Callback collection */
} SDMDebugIf;


/**
 * This function is called by the debugger to start a secure debug session with the remote platform
 *
 * The caller is expected to set the resetType to a value other than link SDM_none when
 * it knows that the debugged platform implements the secure debug certificate processing in its ROM.
 * In the case of SDM_nSRSTReset, if the External COM port driver implementation does not have
 * nSRST capability the caller can provide callbacks pDebugIf->callbacks->f_nSRSTStage1 and
 * pDebugIf->callbacks->f_nSRSTStage2.
 *
 * If the caller wants to get progress indications from SDMInit, then the pDebugIF->callbacks->f_progressIndicationCallback
 * must not be NULL. In such case {@link SDM_Init} will call this callback function with the relevant
 * {@link SDMInitStep}.
 *
 * The pDebugIf->pTopologyDetails parameter is to provide to the Secure Debug Manager and
 * External COM port driver the connection details for the I/O driver. This is I/O driver specific.
 * In the case of the CSAPBCOM I/O driver this should be {@link CSAPBCOMConnectionDescription}.
 *
 * @param[in] resetType Required reset type.
 * @param[in] pDebugIF Connection details for {@link SDM_Init} and {@link EComPort_Init}.
 */
SDM_EXTERN SDMReturnCode SDM_Init(SDMResetType resetType, SDMDebugIf *pDebugIF);

/**
 * This function is called by the debugger to resume the boot of the remote platform.
 * It is typically called after the debugger places its breakpoints at the booting debugged system.
 * It is only useful if the debugged system supports the introduction of debug certificate in the
 * early boot stages, otherwise if the debugged system processes the secure debug certificate at runtime,
 * it does not wait for the resume command.
 *
 * The debugger can know this fact at the response of introduction of the secure debug certificate, which is
 * also the return value of {@link SDM_Init}. In case it happened at early boot then {@link SDM_Init} will
 * return SDM_SUCCESS_WAIT_RESUME. Otherwise if introduced at run time then {@link SDM_Init} will
 * return SDM_SUCCESS.
 *
 * In the 1st case, after the return of {@link SDM_Init} the user set breakpoints while the debugged system
 * is still waiting. After breakpoints are set, the debugger can call {@link SDM_ResumeBoot}
 */
SDM_EXTERN SDMReturnCode SDM_ResumeBoot(void);

/**
 * This function is called by the debugger to end a secure debug session with the remote platform.
 * Function should reset the link then power off the Internal COM Port.
 *
 * @param[in] resetType Required reset type. Should be the same as used with {@link SDM_Init}.
 */
SDM_EXTERN SDMReturnCode SDM_End(SDMResetType resetType);

#ifdef __cplusplus
}
#endif

#endif /* DS5_SECURE_DEBUG_MANAGER_H_ */
