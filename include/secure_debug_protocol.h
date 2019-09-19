// secure_debug_protocol.h
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#ifndef COMMON_SECURE_DEBUG_PROTOCOL_H_
#define COMMON_SECURE_DEBUG_PROTOCOL_H_

#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
#ifdef SDM_EXPORT_SYMBOLS
#define SDM_EXTERN __declspec(dllexport)
#else
#define SDM_EXTERN __declspec(dllimport)
#endif
#else
#define SDM_EXTERN
#endif

#define MAX_MSG_SIZE    (4096 * 2)

typedef enum sdp_rc_t {
    SDP_SUCCESS,
    SDP_FAIL,
} sdp_rc_t;

typedef enum sdp_cert_type_t {
    SDP_CERT0,
    SDP_CERT1,
} sdp_cert_type_t;

typedef uint8_t identification_t[6];
typedef uint8_t soc_id_t[8];
typedef uint8_t nonce_t[32];
typedef uint8_t cert_t[3840];

typedef enum sdp_command_id_t {
    SDP_GET_SOC_ID = 0x1,
    SDP_INT_DEB_CERT = 0x2,
    SDP_RESUME_BOOT = 0x3,
    SDP_DISABLE_PORTS = 0x4,
    SDP_NUM_OF_CMDS
} sdp_command_id_t;

/**
 * Returns the required buffer size to hold the debug certificate, based on the
 * certification type.
 *
 * @param cert_type         [in] Certificate type
 * @return size of the required buffer
 */
size_t SDP_get_cert_data_size(sdp_cert_type_t cert_type);

/**
 * Debugger requests the SoC ID of the debugged system.
 * The debugger must use this value when it builds the debug certificate.
 * The debugged system verifies that the authorized certificate belongs to this SoC by comparing the SoC ID.
 *
 * @param txBuff            [out]    Buffer to prepare protocol command. Cannot be NULL.
 * @param txBuffLen         [in/out] Length of the prepared buffer. Cannot be NULL.
 * @return SDP_SUCCESS on success
 */
sdp_rc_t SDP_form_get_soc_id(uint8_t* txBuff, size_t* txBuffLen);

/**
 * Host response for Get SoC ID command.
 *
 * @param rxBuff         [in]           Input buffer
 * @param rxBuffLen      [in]           Input buffer length
 * @param status         [out]          Parsed status flag
 * @param challenge      [optional out] Optional buffer to capture the challenge nonce
 * @param socId          [out]          Buffer which will contain SoC ID
 * @return SDP_SUCCESS on success
 */
sdp_rc_t SDP_parse_get_soc_id_resp(uint8_t* rxBuff,
                                              size_t rxBuffLen,
                                              uint32_t* status,
                                              nonce_t* challenge,
                                              soc_id_t* socId);

/**
 * Debugger requests the debugged system to authenticate its Debug Certificate
 * which includes the SoC ID and the allowed DCU values for this debug session
 * and optionally the random challenge value it received from the debugged
 * system in the GetSoCIDResp Message.
 * The debug certificate must be based on the host provided SoC ID and the nonce
 * and must include ROT permissions to debug this specific platform
 * (e.g. it holds chain of certificates).
 *
 * @param txBuff            [in]     Buffer to prepare for transmission
 * @param txBuffLen         [in/out] Transmission buffer length, modified to actual length on return
 * @param certType          [in]     The Certificate type should enumerate its type and length
 * @param cert              [in]     Certificate format and its size N are derived from the Certificate type field
 *                                   This may be a certificate chain within one message. Approximate size is 1500 bytes
 * @param certSize          [in]     Size of the cert buffer
 * @return SDP_SUCCESS on success
 */
sdp_rc_t SDP_form_introduce_debug_cert(uint8_t* txBuff,
                                                  size_t* txBuffLen,
                                                  sdp_cert_type_t certType,
                                                  cert_t* cert,
                                                  size_t certSize);

/**
 * The debugged system responds to debugger's Introduce Debug Certificate command
 * with the below message after it analyzed the command and acted upon it.
 *
 * The debugged system may accept or reject the debugger request from the following reasons:
 * •   Incompatible SoCID in the debug certificate.
 * •   Old challenge value in the certificate.
 * •   Bad integrity for the provided debug certificate
 * •   Other?
 *
 * The response includes the current values of the DCUs to let the debugger
 * know what capabilities are now available for him.
 *
 * @param rxBuff            [in]   Input buffer
 * @param rxBuffLen         [in]   Length of input buffer
 * @param status            [out]  Parsed status flag
 * @param waitForResumeBoot [out]     This field is meaningful only when the status is success.
                                         When this flag is set to 1 it tells the debugger that the
                                         debugged system is waiting for a ResumeBoot command to resume its boot.
                                         It is typically set by the ROM implementation of the Secure Debug Handler.
                                         When the Secure Debug feature is implemented in run time,
                                         then this flag is set to 0.
 * @return SDP_SUCCESS on success
 */
sdp_rc_t SDP_parse_introduce_debug_cert_resp(uint8_t* rxBuff,
                                                        size_t rxBuffLen,
                                                        uint32_t* status,
                                                        uint8_t* waitForResumeBoot);

/**
 * This is a message from the debugger to the Secure Debug Handler.
 * It tells the Secure Debug Handler that it should stop using the
 * SDC-600 COM port after it acknowledges this command and resume platform boot.
 *
 * @param txBuff            [in]      Transmission buffer to prepare
 * @param txBuffLen         [in/out]  Length of transmission buffer, modified to actual length on return
 * @return SDP_SUCCESS on success
 */
sdp_rc_t SDP_form_resume_boot(uint8_t* txBuff, size_t* txBuffLen);

/**
 * This is a message from the Secure Debug Handler to the debugger station.
 * It tells the debugger that the Secure Debug Handler stops using the
 * SDC-600 Internal COM port after sending this message and resumes platform boot.
 * Once it is received by the debugger, it will likely disconnect the link
 * and power down the SDC-600 Internal COM Port.
 *
 * @param rxBuff            [in]   Input buffer
 * @param rxBuffLen         [in]   Length of input buffer
 * @param status            [out]  Parsed status flag
 * @return SDP_SUCCESS on success
 */
sdp_rc_t SDP_parse_resume_boot_resp(uint8_t* rxBuff, size_t rxBuffLen, uint32_t* status);

/**
 * This is a message from the debugger to the Secure Debug Handler.
 * It tells the Secure Debug Handler that it should disable the debug ports.
 *
 * @param rxBuff     [in]     Transmission buffer to prepare
 * @param txBuffLen  [in/out] Length of tramission bufer, modified to actual length on return
 * @return SDP_SUCCESS on success
 */
sdp_rc_t SDP_form_disable_debug_ports(uint8_t* txBuff, size_t* txBuffLen);

/**
 * This is a message from the Secure Debug Handler to the debugger.
 * It tells the debugger that the debugged system disabled the debug ports.
 *
 * @param rxBuff            [in]  Input buffer
 * @param rxBuffLen         [in]  Input buffer length
 * @param status            [out] Parsed status flag
 *                                  0 – Success. DCUs are locked
 *                                  1 – Fail. DCUs cannot be locked back at this platform.
 *                                  In order to lock the DCUs the debugged platform must be power on reset.
 * @return SDP_SUCCESS on success
 */
sdp_rc_t SDP_parse_disable_debug_ports_resp(uint8_t* rxBuff, size_t rxBuffLen, uint32_t* status);

#endif /* COMMON_SECURE_DEBUG_PROTOCOL_H_ */
