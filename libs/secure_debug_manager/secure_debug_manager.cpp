// secure_debug_manager.cpp
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#include <stdlib.h>
#include <string.h>

#include "ext_com_port_driver.h"

#include "secure_debug_manager.h"

#include "sdc600_log.h"
#include "secure_debug_types.h"
#include "secure_debug_protocol.h"
#include "auth_token_provider.h"
#include "csapbcom.h"

#define ENTITY_NAME     "SDM"

/******************************************************************************************************
 *
 * Build options
 *
 ******************************************************************************************************/
#define BYPASS_SDM_END  false
#define SDP_CERT_TYPE   0

/******************************************************************************************************
 *
 * Externs and variables
 *
 ******************************************************************************************************/
static uint8_t sdmTxBuff[MAX_MSG_SIZE];
static uint8_t sdmRxBuff[MAX_MSG_SIZE];

static const sd_id_response_buffer_t PROTOCOL         = { 0x01, 0x02, 0x03, 0x04 ,0x05, 0x06 };

CSAPBCOMHandle gHandle;

/******************************************************************************************************
 *
 * static declarations
 *
 ******************************************************************************************************/
static SDMReturnCode SDM_recv_resume_boot_resp(uint32_t* status);
static SDMReturnCode SDM_send_resume_boot(void);
static SDMReturnCode SDM_recv_introduce_debug_cert_resp(uint32_t* status, uint8_t* waitForResumeBoot);
static SDMReturnCode SDM_send_get_soc_id_cmd(void);
static SDMReturnCode SDM_send_introduce_debug_cert(sdp_cert_type_t cert_type, cert_t* cert, size_t certLen);
static SDMReturnCode SDM_recv_get_soc_id_cmd(soc_id_t* socIdBuf,
                                        nonce_t* challengeBuf);
static SDMReturnCode SDM_generate_cert(soc_id_t* socIdBuf,
                                  nonce_t* challengeBuf,
                                  sdp_cert_type_t cert_type,
                                  cert_t* cert,
                                  size_t* certLen,
                                  SDMDebugIf* pDebugIF);
static int SDM_check_protocol(sd_id_response_buffer_t idResBuff, const sd_id_response_buffer_t prot_id);

/* utility functions */
static SDMReturnCode SDM_rx(uint8_t* RxBuffer, size_t RxBufferLength, size_t* actualLength);
static SDMReturnCode SDM_tx(uint8_t* TxBuffer, size_t TxBufferLength, size_t* actualLength);

/******************************************************************************************************
 *
 * private
 *
 ******************************************************************************************************/
static SDMReturnCode SDM_recv_resume_boot_resp(uint32_t* status)
{
    SDMReturnCode res = SDM_SUCCESS;
    size_t actualLength = 0;

    /* validate pointers */
    SDC600_ASSERT_ERROR(status != NULL, true, SDM_FAIL_INTERNAL);

    SDC600_ASSERT(SDM_rx(sdmRxBuff, sizeof(sdmTxBuff), &actualLength), SDM_SUCCESS);

    SDC600_LOG_BUF("<---------", sdmRxBuff, actualLength, "resume boot");

    SDC600_ASSERT_ERROR(SDP_parse_resume_boot_resp(sdmRxBuff, actualLength, status),
        SDP_SUCCESS,
        SDM_FAIL_UNEXPECTED_SYMBOL);

bail:

    return res;
}

static SDMReturnCode SDM_send_resume_boot(void)
{
    SDMReturnCode res = SDM_SUCCESS;
    size_t txBuffLen = sizeof(sdmTxBuff);
    size_t actualLength = 0;

    SDC600_ASSERT_ERROR(SDP_form_resume_boot(sdmTxBuff, &txBuffLen),
        SDP_SUCCESS,
        SDM_FAIL_INTERNAL);

    SDC600_LOG_INFO("--------->", "RESUME_BOOT\n");

    SDC600_ASSERT(SDM_tx(sdmTxBuff, txBuffLen, &actualLength), SDM_SUCCESS);

bail:
    return res;
}

static SDMReturnCode SDM_recv_introduce_debug_cert_resp(uint32_t* status, uint8_t* waitForResumeBoot)
{
    SDMReturnCode res = SDM_SUCCESS;
    size_t actualLength = 0;

    /* validate pointers */
    SDC600_ASSERT_ERROR(status != NULL, true, SDM_FAIL_INTERNAL);
    SDC600_ASSERT_ERROR(waitForResumeBoot != NULL, true, SDM_FAIL_INTERNAL);

    SDC600_ASSERT(SDM_rx(sdmRxBuff, sizeof(sdmTxBuff), &actualLength), SDM_SUCCESS);

    SDC600_LOG_BUF("<---------", sdmRxBuff, actualLength, "debug_certificate");

    SDC600_ASSERT_ERROR(SDP_parse_introduce_debug_cert_resp(sdmRxBuff,
                                                         sizeof(sdmRxBuff),
                                                         status,
                                                         waitForResumeBoot),
        SDP_SUCCESS,
        SDM_FAIL_UNEXPECTED_SYMBOL);

bail:

    return res;
}


static SDMReturnCode SDM_send_introduce_debug_cert(sdp_cert_type_t cert_type, cert_t* cert, size_t certLen)
{
    SDMReturnCode res = SDM_SUCCESS;

    size_t txBuffLen = sizeof(sdmTxBuff);
    size_t actualLength = 0;

    /* validate pointers */
    SDC600_ASSERT_ERROR(cert != NULL, true, SDM_FAIL_INTERNAL);

    SDC600_ASSERT_ERROR(SDP_form_introduce_debug_cert(sdmTxBuff,
                                                   &txBuffLen,
                                                   cert_type,
                                                   cert,
                                                   certLen),
        SDP_SUCCESS,
        SDM_FAIL_INTERNAL);

    SDC600_LOG_INFO("--------->", "INTRODUCE_DEBUG_CERT\n");

    SDC600_ASSERT(SDM_tx(sdmTxBuff, txBuffLen, &actualLength), SDM_SUCCESS);

bail:
    return res;
}

static SDMReturnCode SDM_generate_cert(soc_id_t* socIdBuf,
                                  nonce_t* challengeBuf,
                                  sdp_cert_type_t cert_type,
                                  cert_t* cert,
                                  size_t* certLen,
                                  SDMDebugIf* pDebugIf)
{
    SDMReturnCode res = SDM_SUCCESS;
    ATPReturnCode atp_res = ATP_SUCCESS;

    atp_res = ATP_GenerateSecureDebugCertificate((char *)socIdBuf, sizeof(*socIdBuf),
                                             (char *)challengeBuf, sizeof(*challengeBuf),
                                             (char *)cert, sizeof(*cert), certLen,
                                             pDebugIf);

    SDC600_ASSERT_ERROR(atp_res == ATP_SUCCESS, true, SDM_FAIL_USER_CRED);

bail:
    return res;
}

static SDMReturnCode SDM_recv_get_soc_id_cmd(soc_id_t* socIdBuf,
                                             nonce_t* challengeBuf)
{
    SDMReturnCode res = SDM_SUCCESS;
    size_t actualLength = 0;
    uint32_t status = 0;

    /* validate pointers */
    SDC600_ASSERT_ERROR(socIdBuf != NULL, true, SDM_FAIL_INTERNAL);
    SDC600_ASSERT_ERROR(challengeBuf != NULL, true, SDM_FAIL_INTERNAL);

    SDC600_ASSERT(SDM_rx(sdmRxBuff, sizeof(sdmTxBuff), &actualLength), SDM_SUCCESS);

    SDC600_LOG_BUF("<---------", sdmRxBuff, actualLength, "get_soc_id");

    SDC600_ASSERT_ERROR(SDP_parse_get_soc_id_resp(sdmRxBuff,
                                               actualLength,
                                               &status,
                                               challengeBuf,
                                               socIdBuf),
        SDM_SUCCESS,
        SDM_FAIL_UNEXPECTED_SYMBOL);

    SDC600_ASSERT_ERROR(status == 0, true, SDM_FAIL_USER_CRED);

bail:
    return res;
}

static SDMReturnCode SDM_send_get_soc_id_cmd(void)
{
    SDMReturnCode res = SDM_SUCCESS;

    size_t txBuffLen = sizeof(sdmTxBuff);
    size_t actualLength = 0;

    SDC600_ASSERT_ERROR(SDP_form_get_soc_id(sdmTxBuff, &txBuffLen),
        SDP_SUCCESS,
        SDM_FAIL_INTERNAL);

    SDC600_LOG_INFO("--------->", "GET_SOC_ID\n");

    SDC600_ASSERT(SDM_tx(sdmTxBuff, txBuffLen, &actualLength), SDM_SUCCESS);

bail:
    return res;
}

static int SDM_check_protocol(sd_id_response_buffer_t idResBuff, const sd_id_response_buffer_t prot_id)
{
    SDC600_LOG_BUF(ENTITY_NAME, idResBuff, SD_RESPONSE_LENGTH, "idResBuff");
    SDC600_LOG_BUF(ENTITY_NAME, prot_id, SD_RESPONSE_LENGTH, "prot_id");

    /* check if protocol id matches */
    if (memcmp(prot_id, idResBuff, SD_RESPONSE_LENGTH) != 0)
    {
        SDC600_LOG_ERR(ENTITY_NAME, "protocol mismatch\n");
        return SDM_FAIL_UNSUPPORTED_PROTOCOL_ID;
    }

    return SDM_SUCCESS;
}

static SDMReturnCode SDM_ecpdErrorConvert(ECPDReturnCode err)
{
    switch (err)
    {
        case ECPD_SUCCESS:
            return SDM_SUCCESS;
        case ECPD_TIMEOUT:
            return SDM_FAIL_NO_RESPONSE;
        case ECPD_UNEXPECTED_SYMBOL:
            return SDM_FAIL_UNEXPECTED_SYMBOL;
        case ECPD_LINK_ERR:
            return SDM_FAIL_NO_RESPONSE;
        case ECPD_NO_INTI:
        case ECPD_BUFFER_OVERFLOW:
        case ECPD_TX_FAIL:
        case ECPD_RX_FAIL:
        default:
            return SDM_FAIL_IO;
    }
}

static SDMReturnCode SDM_tx(uint8_t* TxBuffer, size_t TxBufferLength, size_t* actualLength)
{
    ECPDReturnCode ecpdRes = ECPD_SUCCESS;

    ecpdRes = EComPort_Tx(TxBuffer, TxBufferLength, actualLength);
    if (ecpdRes != ECPD_SUCCESS)
    {
        SDC600_LOG_ERR(ENTITY_NAME, "EComPort_Tx failed [0x%04x]\n", ecpdRes);
        return SDM_ecpdErrorConvert(ecpdRes);
    }

    return SDM_SUCCESS;
}

static SDMReturnCode SDM_rx(uint8_t* RxBuffer, size_t RxBufferLength, size_t* actualLength)
{
    ECPDReturnCode ecpdRes = ECPD_SUCCESS;

    ecpdRes = EComPort_Rx(RxBuffer, RxBufferLength, actualLength);
    if (ecpdRes != ECPD_SUCCESS)
    {
        SDC600_LOG_ERR(ENTITY_NAME, "EComPort_Rx failed [0x%04x]\n", ecpdRes);
        return SDM_ecpdErrorConvert(ecpdRes);
    }

    return SDM_SUCCESS;
}

/******************************************************************************************************
 *
 * public
 *
 ******************************************************************************************************/
SDMReturnCode SDM_Init(SDMResetType resetType, SDMDebugIf* pDebugIF)
{
    SDMReturnCode res = SDM_SUCCESS;
    ECPDReturnCode ecpdRes = ECPD_SUCCESS;
    sd_id_response_buffer_t idResBuff;

    /* just to shorted the line length */
    f_progressIndicationCallback progIndFunc = pDebugIF->callbacks->f_progressIndicationCallbackFunc;

    soc_id_t* socIdBuf = NULL;
    size_t socIdBufLen = 0;

    nonce_t* challengeBuf = NULL;
    size_t challengeBufLen = 0;

    cert_t* cert = NULL;
    size_t certLen = 0;

    uint32_t status = 0;
    uint8_t waitForResumeBoot = 0;

    SDMDebugIf* tmpDebugIf = NULL;

    /*
    * First, open a connection to the CSAPBCOM library and the debug vehicle
    */
    SDC600_ASSERT_ERROR(CSAPBCOM_Open(&gHandle, (CSAPBCOMConnectionDescription*) pDebugIF->pTopologyDetails), CSAPBCOM_SUCCESS, SDM_FAIL_IO);

    /*
    * Connect to the Debug Access Port (DAP)
    */
    SDC600_ASSERT_ERROR(CSAPBCOM_Connect(gHandle), CSAPBCOM_SUCCESS, SDM_FAIL_IO);

    // Use SDMDebugIf->pTopologyDetails details to pass the CSAPBCOM handle to Ext COM port driver
    // Create full copy to preserve callers ConnectionDescription
    tmpDebugIf = (SDMDebugIf*) malloc(sizeof(SDMDebugIf));
    SDC600_ASSERT_ERROR(tmpDebugIf != NULL, true, SDM_FAIL_INTERNAL);
    tmpDebugIf->callbacks = pDebugIF->callbacks;
    tmpDebugIf->pTopologyDetails = &gHandle;
    tmpDebugIf->version = pDebugIF->version;

    // SDMInit calls the EComPort_Init.
    // Upon fail, exit with the fail code.
    ecpdRes = EComPort_Init(resetType, idResBuff, sizeof(idResBuff), tmpDebugIf);
    if (ecpdRes != ECPD_SUCCESS)
    {
        SDC600_LOG_ERR(ENTITY_NAME, "EComPort_Init failed [0x%04x]\n", ecpdRes);
        res = SDM_ecpdErrorConvert(ecpdRes);
        goto bail;
    }


    // If pDebugIF->f_ProgressIndicationCallback is not NULL
    // call pDebugIF->f_ProgressIndicationCallback with SDMInitStep = SDM_COM_Port_Init_Done with the current percent complete value
    if (progIndFunc != NULL)
        progIndFunc(SDM_COM_Port_Init_Done, 10);

    // Upon success the IDResponseBuffer will hold the IDA response of the remote platform (6 bytes).
    // This is used to detect what high level protocol the remote system supports.
    // In the secure debug case the protocol is ‘TBD’.
    // Verify the IDA response value to be as expected (TBD),
    // otherwise fail and return with a proper error code (unsupported remote platform protocol ID).
    SDC600_ASSERT_ERROR(SDM_check_protocol(idResBuff, PROTOCOL), 0, SDM_FAIL_UNSUPPORTED_PROTOCOL_ID);

    // If pDebugIF->f_ProgressIndicationCallback is not NULL
    //call pDebugIF->f_ProgressIndicationCallback with SDMInitStep = SDM_Received_Expected_IDA_Response with the current percent complete value
    if (progIndFunc != NULL)
        progIndFunc(SDM_Received_Expected_IDA_Response, 20);

    // Form a Get SoC ID command.
    // Call the EComPort_Tx to transmit the command to the debugged system.
    SDC600_ASSERT(SDM_send_get_soc_id_cmd(), SDM_SUCCESS);

    // If pDebugIF->f_ProgressIndicationCallback is not NULL
    // call pDebugIF->f_ProgressIndicationCallback with SDMInitStep = SDM_Sent_Get_SOC_ID with the current percent complete value
    if (progIndFunc != NULL)
        progIndFunc(SDM_Sent_Get_SOC_ID, 30);

    // Allocate a receive buffer (of MAX size, or of exact expected response size).
    socIdBuf = (soc_id_t *) calloc(1, sizeof(soc_id_t));
    SDC600_ASSERT_ERROR(socIdBuf != NULL, true, SDM_FAIL_INTERNAL);

    challengeBuf = (nonce_t *) calloc(1, sizeof(nonce_t));
    SDC600_ASSERT_ERROR(challengeBuf != NULL, true, SDM_FAIL_INTERNAL);

    // call EComPort_Rx.
    // Upon return, parse and analyze the debugged system response.
    // Extract the SoCID field and the nonce (it is the certificate freshness challenge).
    SDC600_ASSERT(SDM_recv_get_soc_id_cmd(socIdBuf, challengeBuf),
               SDM_SUCCESS);

    // If pDebugIF->f_ProgressIndicationCallback is not NULL
    // call pDebugIF->f_ProgressIndicationCallback with SDMInitStep = SDM_Received_SOC_ID with the current percent complete value
    if (progIndFunc != NULL)
        progIndFunc(SDM_Received_SOC_ID, 40);

    // Call GenerateSecureDebugCertificate function with the SoCID and the nonce.
    cert = (cert_t *) calloc(1, sizeof(cert_t));
    SDC600_ASSERT_ERROR(cert != NULL, true, SDM_FAIL_INTERNAL);
    certLen = sizeof(cert_t);

    // A chain of certificates is provided in one chunk (assumed size is 3KB).
    SDC600_ASSERT_ERROR(SDM_generate_cert(socIdBuf, challengeBuf, (sdp_cert_type_t) SDP_CERT_TYPE, cert, &certLen, pDebugIF),
                     SDM_SUCCESS,
                     SDM_FAIL_USER_CRED);

    // If pDebugIF->f_ProgressIndicationCallback is not NULL
    // call pDebugIF->f_ProgressIndicationCallback with SDMInitStep = SDM_Received_Secure_Debug_Certificate with the current percent complete value
    if (progIndFunc != NULL)
        progIndFunc(SDM_Received_Secure_Debug_Certificate, 50);

    // Form an Introduce Secure Debug Certificate command with the calculated secure debug certificate.
    // Call the EComPort_Tx to transmit the command to the debugged system.
    SDC600_ASSERT(SDM_send_introduce_debug_cert((sdp_cert_type_t)SDP_CERT_TYPE, cert, certLen), SDM_SUCCESS);

    // If pDebugIF->f_ProgressIndicationCallback is not NULL
    // call pDebugIF->f_ProgressIndicationCallback with SDMInitStep = SDM_Sent_Secure_Debug_Certificate with the current percent complete value
    if (progIndFunc != NULL)
        progIndFunc(SDM_Sent_Secure_Debug_Certificate, 60);

    // Allocate a receive buffer (of MAX size, or of exact expected response size).
    // call EComPort_Rx.
    SDC600_ASSERT(SDM_recv_introduce_debug_cert_resp(&status, &waitForResumeBoot), SDM_SUCCESS);


    //19. If pDebugIF->f_ProgressIndicationCallback is not NULL
    //call pDebugIF->f_ProgressIndicationCallback with SDMInitStep = SDM_Complete with the current percent complete value = 100
    if (progIndFunc != NULL)
        progIndFunc(SDM_Complete, 100);

    // Extract the returned status code.
    SDC600_ASSERT_ERROR(status == 0, true, SDM_FAIL_USER_CRED);

    // Return with the extracted success/fail code

    // Note: the response may be just a ‘Success’ if the debugged system processed
    // the secure debug certificate at its run time code, or ‘Success, debugged
    // system waits for resume its boot’ if the debugged system processed the secure
    // debug certificate at early boot (ROM implementation).

bail:

    if (tmpDebugIf != NULL)
        free(tmpDebugIf);

    if (socIdBuf != NULL)
        free(socIdBuf);

    if (challengeBuf != NULL)
        free(challengeBuf);

    if (cert != NULL)
        free(cert);

    if (res == SDM_SUCCESS && waitForResumeBoot != 0)
        res = SDM_SUCCESS_WAIT_RESUME;

    CSAPBCOM_Disconnect(gHandle);

    if ((res != SDM_SUCCESS) && (res != SDM_SUCCESS_WAIT_RESUME))
    {
        CSAPBCOM_Close(gHandle);
    }

    return res;
}

SDMReturnCode SDM_ResumeBoot(void)
{
    SDMReturnCode res = SDM_SUCCESS;
    uint32_t status = 0;

    /*
    * Connect to the APBCOM
    */
    SDC600_ASSERT_ERROR(CSAPBCOM_Connect(gHandle), CSAPBCOM_SUCCESS, SDM_FAIL_IO);

    // 1.  Form a Resume Boot command.
    // 2.  Call the EComPort_Tx to transmit the command to the debugged system.
    // 3.  Allocate a receive buffer (of MAX size, or of exact expected response size).
    SDC600_ASSERT(SDM_send_resume_boot(), SDM_SUCCESS);

    // 4.  call EComPort_Rx.
    SDC600_ASSERT(SDM_recv_resume_boot_resp(&status), SDM_SUCCESS);

    // 5.  Verify the response status from the target system
    // 6.  Return with success/fail code
    SDC600_ASSERT_ERROR(status == 0, true, SDM_FAIL_UNEXPECTED_SYMBOL);

bail:

    CSAPBCOM_Disconnect(gHandle);

    if (res != SDM_SUCCESS)
    {
        CSAPBCOM_Close(gHandle);
    }

    return res;
}

SDMReturnCode SDM_End(SDMResetType ResetType)
{
    SDMReturnCode res = SDM_SUCCESS;
    ECPDReturnCode ecpdRes = ECPD_SUCCESS;

    /*
    * Connect to the APBCOM
    */
    SDC600_ASSERT_ERROR(CSAPBCOM_Connect(gHandle), CSAPBCOM_SUCCESS, SDM_FAIL_IO);

#if BYPASS_SDM_END == true
    SDC600_LOG_WARN(ENTITY_NAME, "SDM_End is bypassed\n");
    goto bail;
#endif

    // 1.  Future feature: Send to the debugged system “disable debug ports” command to securely
    //     close the debug session. It will not work in Cerberus in many platforms where the
    //     ROM locks DCUs at ROM exit. However, in Alcatraz it can work. In non Alcatraz platforms
    //     (Cerberus only) disabling of the debug ports when the ROM locks the DCUs can be implemented
    //     by calling SDMEnd with RemoteReboot flag set to TRUE. In this case the DCUs will return to
    //     their default values and ROM exit will lock the DCUs.

    // Disable the link:
    // 2.  transmits LPH2RL flag to the External COM port TX.

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

    // 3.  EComPort_Power(PowerOff);
    ecpdRes = EComPort_Power(ECPD_POWER_OFF);
    if (ecpdRes != ECPD_SUCCESS)
    {
        SDC600_LOG_ERR(ENTITY_NAME, "EComPort_Power failed [0x%04x]\n", ecpdRes);
        res = SDM_ecpdErrorConvert(ecpdRes);
        goto bail;
    }

    // 4.  If the RemoteReboot flag is TRUE, call EComPortRReboot
    if (ResetType == SDM_COMPortReset)
    {
        ecpdRes = EComPort_RReboot();
        if (ecpdRes != ECPD_SUCCESS)
        {
            SDC600_LOG_ERR(ENTITY_NAME, "EComPortRReboot failed [0x%04x]\n", ecpdRes);
            res = SDM_ecpdErrorConvert(ecpdRes);
            goto bail;
        }
    }

bail:
    CSAPBCOM_Disconnect(gHandle);
    CSAPBCOM_Close(gHandle);

    return res;
}
