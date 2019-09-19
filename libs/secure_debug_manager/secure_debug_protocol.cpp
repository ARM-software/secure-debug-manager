// secure_debug_protocol.cpp
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#include <stdio.h>
#include <string.h>

#include "secure_debug_protocol.h"
#include "sdc600_log.h"

/******************************************************************************************************
 *
 * Macros
 *
 ******************************************************************************************************/
#define ENTITY_NAME     "SDP"

/**
 * Returns the required buffer size to hold the debug certificate, based on the
 * certification type.
 *
 * @param cert_type         [in] Certificate type
 * @return size of the required buffer
 */
size_t SDP_get_cert_data_size(sdp_cert_type_t cert_type)
{
    switch (cert_type) {
        case SDP_CERT0:
            return 1720;
        case SDP_CERT1:
            return 2327;
        default:
            return 0;
    }
}

/**
 * Debugger requests the SoC ID of the debugged system.
 * The debugger must use this value when it builds the debug certificate.
 * The debugged system verifies that the authorized certificate belongs to this SoC by comparing the SoC ID.
 *
 * @param txBuff            [out]    Buffer to prepare protocol command.
 * @param txBuffLen         [in/out] Length of the prepared buffer. Modified to the actual length on return.
 * @return SDP_SUCCESS on success
 */
sdp_rc_t SDP_form_get_soc_id(uint8_t* txBuff, size_t* txBuffLen)
{
    sdp_rc_t res = SDP_SUCCESS;
    size_t originalTxBuffLen = 0;

    SDC600_ASSERT_ERROR(txBuff != NULL, true, SDP_FAIL);
    SDC600_ASSERT_ERROR(txBuffLen != NULL, true, SDP_FAIL);
    originalTxBuffLen = *txBuffLen;
    *txBuffLen = 3;
    SDC600_ASSERT_ERROR(originalTxBuffLen >= 3, true, SDP_FAIL);

    txBuff[0] = SDP_GET_SOC_ID;     /* command */
    memset(txBuff + 1, 0, 2);       /* len */

bail:
    return res;
}

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
                                   soc_id_t* socId)
{
    sdp_rc_t res = SDP_SUCCESS;

    const size_t MSG_SIZE = 65;
    uint16_t len = 0;

    /* validate pointers */
    SDC600_ASSERT_ERROR(rxBuff != NULL, true, SDP_FAIL);
    SDC600_ASSERT_ERROR(status != NULL, true, SDP_FAIL);
    SDC600_ASSERT_ERROR(socId != NULL, true, SDP_FAIL);

    /* check data sizes */
    SDC600_ASSERT_ERROR(rxBuffLen >= 36 + sizeof(soc_id_t), true, SDP_FAIL);

    len = rxBuff[1] | rxBuff[2] << 8;

    SDC600_ASSERT_ERROR(len == MSG_SIZE, true, SDP_FAIL);

    *status = rxBuff[3];                            /* status */
    if (challenge != NULL)
    {
        memcpy((uint8_t *)challenge, rxBuff + 4, sizeof(nonce_t));/* nonce */
    }

    memcpy((uint8_t *)socId, rxBuff + 36, sizeof(soc_id_t));         /* soc_id */

bail:
    return res;
}

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
                                       size_t certSize)
{
    sdp_rc_t res = SDP_SUCCESS;

    uint16_t len = 0;

    /* validate pointers */
    SDC600_ASSERT_ERROR(txBuff != NULL, true, SDP_FAIL);
    SDC600_ASSERT_ERROR(txBuffLen != NULL, true, SDP_FAIL);
    SDC600_ASSERT_ERROR(cert != NULL, true, SDP_FAIL);


    SDC600_LOG_DEBUG(ENTITY_NAME, "certSize[%zu] cert_t[%zu] certType[%u]\n", certSize, sizeof(cert_t), certType);

    /* check data sizes */
    SDC600_ASSERT_ERROR(certSize <= sizeof(cert_t), true, SDP_FAIL);
    SDC600_ASSERT_ERROR(*txBuffLen >= 4 + certSize, true, SDP_FAIL);

    len = 1 + certSize;

    memset(txBuff, 0, 3 + len);

    txBuff[0] = SDP_INT_DEB_CERT;                   /* command */
    txBuff[1] = len & 0xFF;                         /* length */
    txBuff[2] = (len & 0xFF00) >> 8;
    txBuff[3] = certType;                           /* cert_type */
    memcpy(txBuff + 4, cert, certSize);             /* certifcate */

    *txBuffLen = 3 + len;

bail:
    return res;
}

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
                                             uint8_t* waitForResumeBoot)
{
    sdp_rc_t res = SDP_SUCCESS;

    const size_t MSG_SIZE = 2;
    uint16_t len = 0;

    /* validate pointers */
    SDC600_ASSERT_ERROR(rxBuff != NULL, true, SDP_FAIL);
    SDC600_ASSERT_ERROR(status != NULL, true, SDP_FAIL);
    SDC600_ASSERT_ERROR(waitForResumeBoot != NULL, true, SDP_FAIL);

    SDC600_ASSERT_ERROR(rxBuffLen >= 5, true, SDP_FAIL);

    len = rxBuff[1] | rxBuff[2] << 8;
    SDC600_ASSERT_ERROR(len == MSG_SIZE, true, SDP_FAIL);

    *status = rxBuff[3];                            /* status */
    *waitForResumeBoot = rxBuff[4];                 /* waitForResumeBoot */

bail:
    return res;
}

/**
 * This is a message from the debugger to the Secure Debug Handler.
 * It tells the Secure Debug Handler that it should stop using the
 * SDC-600 COM port after it acknowledges this command and resume platform boot.
 *
 * @param txBuff            [in]      Transmission buffer to prepare
 * @param txBuffLen         [in/out]  Length of transmission buffer, modified to actual length on return
 * @return SDP_SUCCESS on success
 */
sdp_rc_t SDP_form_resume_boot(uint8_t* txBuff, size_t* txBuffLen)
{
    sdp_rc_t res = SDP_SUCCESS;

    SDC600_ASSERT_ERROR(txBuff != NULL, true, SDP_FAIL);
    SDC600_ASSERT_ERROR(txBuffLen != NULL, true, SDP_FAIL);
    SDC600_ASSERT_ERROR(*txBuffLen >= 3, true, SDP_FAIL);

    txBuff[0] = SDP_RESUME_BOOT;            /* command */
    memset(txBuff + 1, 0, 2);               /* len */

    *txBuffLen = 3;

bail:
    return res;
}

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
sdp_rc_t SDP_parse_resume_boot_resp(uint8_t* rxBuff, size_t rxBuffLen, uint32_t* status)
{
    sdp_rc_t res = SDP_SUCCESS;

    const size_t MSG_SIZE = 1;
    uint16_t len = 0;

    /* validate pointers */
    SDC600_ASSERT_ERROR(rxBuff != NULL, true, SDP_FAIL);
    SDC600_ASSERT_ERROR(status != NULL, true, SDP_FAIL);

    SDC600_ASSERT_ERROR(rxBuff[0] == SDP_RESUME_BOOT, true, SDP_FAIL);
    SDC600_ASSERT_ERROR(rxBuffLen >= 3, true, SDP_FAIL);

    len = rxBuff[1] | rxBuff[2] << 8;
    SDC600_ASSERT_ERROR(len == MSG_SIZE, true, SDP_FAIL);

    *status = rxBuff[3];                            /* status */

bail:
    return res;
}

/**
 * This is a message from the debugger to the Secure Debug Handler.
 * It tells the Secure Debug Handler that it should disable the debug ports.
 *
 * @param rxBuff     [in]     Transmission buffer to prepare
 * @param txBuffLen  [in/out] Length of tramission bufer, modified to actual length on return
 * @return SDP_SUCCESS on success
 */
sdp_rc_t SDP_form_disable_debug_ports(uint8_t* txBuff, size_t* txBuffLen)
{
    sdp_rc_t res = SDP_SUCCESS;

    SDC600_ASSERT_ERROR(txBuff != NULL, true, SDP_FAIL);
    SDC600_ASSERT_ERROR(txBuffLen != NULL, true, SDP_FAIL);

    SDC600_ASSERT_ERROR(*txBuffLen >= 3, true, SDP_FAIL);

    txBuff[0] = SDP_DISABLE_PORTS;          /* command */
    memset(txBuff + 1, 0, 2);               /* len */

    *txBuffLen = 3;

bail:
    return res;
}

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
sdp_rc_t SDP_parse_disable_debug_ports_resp(uint8_t* rxBuff, size_t rxBuffLen, uint32_t* status)
{
    sdp_rc_t res = SDP_SUCCESS;

    const size_t MSG_SIZE = 1;
    uint16_t len = 0;

    /* validate pointers */
    SDC600_ASSERT_ERROR(rxBuff != NULL, true, SDP_FAIL);
    SDC600_ASSERT_ERROR(status != NULL, true, SDP_FAIL);

    SDC600_ASSERT_ERROR(rxBuff[0] == SDP_DISABLE_PORTS, true, SDP_FAIL);
    SDC600_ASSERT_ERROR(rxBuffLen >= 3, true, SDP_FAIL);

    len = rxBuff[1] | rxBuff[2] << 8;
    SDC600_ASSERT_ERROR(len == MSG_SIZE, true, SDP_FAIL);

    *status = rxBuff[3];                            /* status */

bail:
    return res;
}
