// auth_token_provider.h
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

 /**
 * \file
 *
 * \brief This header file is the entry point to Authentication
 * Token Provider functionality and includes all necessary definitions.
 */

#ifndef LIBS_AUTH_TOKEN_PROVIDER_AUTH_TOKEN_PROVIDER_H_
#define LIBS_AUTH_TOKEN_PROVIDER_AUTH_TOKEN_PROVIDER_H_

#ifdef __cplusplus
extern "C"{
#endif

#ifdef _WIN32
#ifdef AUTH_TOKEN_PROVIDER_EXPORT_SYMBOLS
#define AUTH_TOKEN_PROVIDER_EXTERN __declspec(dllexport)
#else
#define AUTH_TOKEN_PROVIDER_EXTERN __declspec(dllimport)
#endif
#else
#define AUTH_TOKEN_PROVIDER_EXTERN
#endif

#include "secure_debug_manager.h"

/**
* \brief Return codes for GenerateSecureDebugCertificate() functionality.
*/
AUTH_TOKEN_PROVIDER_EXTERN
typedef enum ATPReturnCode
{
    ATP_SUCCESS, /*!< Success, no error */
    ATP_FAIL_NO_USER_CRED,  /*!< Invalid user credentials for the debugged platform */
    ATP_FAIL_UNSUPP_SOCID,  /*!< Unsupported remote platform SoCID */
    ATP_FAIL_SHORT_CERT_BUFFER, /*!< Provided buffer is too small to receive certificate */
    ATP_FAIL,  /*!<  General fail */
} ATPReturnCode;

/**
 * \brief Generate secure debug certificate
 *
 * This function may be terminated on the debugger platform or may require
 * communication to authorization server. It would need to verify the user’s
 * credentials and grant full, or partial or no debug capabilities for the target
 * platform per the provided SoCID. If the user does not have rights to debug the
 * platform then a proper error code will be returned. If approved, a certificate
 * is returned.
 *
 * Note: It may take a fairly long time for this function to complete. While this function
 * makes progress, it should call periodically the ProgressIndicationCallback with a
 * progressive PercentComplete for the sake of the debugger’s user interface.
 * If pDebugIF->callbacks->f_ProgressIndicationCallback is not NULL,
 * pDebugIF->callbacks->f_ProgressIndicationCallback should be called with {@link SDMInitStep}
 * SDM_Creating_Secure_Debug_Certificate with the current percent complete value.
 *
 * @param[in]  pSocId SoC ID of the debugged system. Received by the Get SoC ID command.
 * @param[in]  socIdLen Size in bytes of the SoC ID.
 * @param[in]  pChallenge Nonce that is randomized by the debugged system when it
 *                       received the Get SoC ID command. The nonce must be used
 *                       while signing the debug certificate with the SoCID. This response
 *                       will not be sent by the debugged system if the IDR command was
 *                       not issued. When the debugged system does not support
 *                       challenge response, it fills this field with zeroes.
 * @param[in]  challengeLen Size in bytes of challenge.
 * @param[out] pCertificateBuffer A client supplied buffer to receive the certificate data.
 * @param[in]  bufferLen Size in bytes of the client supplied certificate buffer.
 * @param[out] actualBufferLen Updated with the number of bytes used in the certificate data buffer.
                               It should be less or equal to bufferLen.
 * @param[in]  pDebugIf {@link SDM_Init} connection details. Only used for progress reporting callbacks.
*/
AUTH_TOKEN_PROVIDER_EXTERN
ATPReturnCode ATP_GenerateSecureDebugCertificate (char* pSocId, size_t socIdLen,
                                         char* pChallenge, size_t challengeLen,
                                         char* pCertificateBuffer, size_t bufferLen, size_t* actualBufferLen,
                                         SDMDebugIf* pDebugIf);

#ifdef __cplusplus
}
#endif
#endif /* LIBS_AUTH_TOKEN_PROVIDER_AUTH_TOKEN_PROVIDER_H_ */
