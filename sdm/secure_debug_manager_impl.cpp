// secure_debug_manager_impl.cpp
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#include <stdlib.h>
#include <string.h>

#include "secure_debug_manager_impl.h"
#include "secure_debug_manager.h"
#include "ext_com_port_driver.h"
#include "sdm_config.h"

#include "psa_adac_sdm.h"
#include "psa_adac_debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <regex>

#define ENTITY_NAME     "SDM"
#define SD_RESPONSE_LENGTH  6

static const uint8_t PROTOCOL[] = { 0x50, 0x53, 0x41, 0x44, 0x42, 0x47 };

namespace {

    SDMReturnCode CheckProtocol(uint8_t *idResBuff, const uint8_t *prot_id)
    {
        PSA_ADAC_LOG_DUMP(ENTITY_NAME, "idResBuff", idResBuff, SD_RESPONSE_LENGTH);
        PSA_ADAC_LOG_DUMP(ENTITY_NAME, "prot_id", prot_id, SD_RESPONSE_LENGTH);

        // check if protocol id matches 
        if (memcmp(prot_id, idResBuff, SD_RESPONSE_LENGTH) != 0)
        {
            PSA_ADAC_LOG_ERR(ENTITY_NAME, "CheckProtocol: protocol mismatch\n");
            return SDMReturnCode_UnsupportedOperation;
        }

        return SDMReturnCode_Success;
    }

    std::string userInputStringTrim(const char *inputText)
    {
        std::string text(inputText);
        text = std::regex_replace(text, std::regex("\\s+$"), std::string(""));
        text = std::regex_replace(text, std::regex("^\\s+"), std::string(""));

        return text;
    }

}

/******************************************************************************************************
 *
 * public
 *
 ******************************************************************************************************/

SecureDebugManagerImpl::SecureDebugManagerImpl() : mOpen(false)
{
}

SecureDebugManagerImpl::~SecureDebugManagerImpl()
{
}

SDMReturnCode SecureDebugManagerImpl::SDMOpen(const SDMOpenParameters* params)
{
    if (mOpen)
    {
        return SDMReturnCode_InternalError;
    }

    if (params == 0)
    {
        return SDMReturnCode_InvalidArgument;
    }

#ifdef SDM_CONFIG_COM_DEVICE_MEMAP_ADDRESS
    SDMDeviceDescriptor comPortDeviceMemAp;
    comPortDeviceMemAp.deviceType = SDMDeviceType_ArmADI_AP;
    comPortDeviceMemAp.armAP.dpIndex = SDM_CONFIG_COM_DEVICE_DP_INDEX;
    comPortDeviceMemAp.armAP.address = SDM_CONFIG_COM_DEVICE_MEMAP_ADDRESS;
#endif

    SDMDeviceDescriptor comPortDevice;
    comPortDevice.deviceType = SDM_CONFIG_COM_DEVICE_TYPE;
    if (comPortDevice.deviceType == SDMDeviceType_ArmADI_AP)
    {
        comPortDevice.armAP.dpIndex = SDM_CONFIG_COM_DEVICE_DP_INDEX;
        comPortDevice.armAP.address = SDM_CONFIG_COM_DEVICE_ADDRESS;
    }
    else
    {
        comPortDevice.armCoreSightComponent.dpIndex = SDM_CONFIG_COM_DEVICE_DP_INDEX;
        comPortDevice.armCoreSightComponent.baseAddress = SDM_CONFIG_COM_DEVICE_ADDRESS;
#ifdef SDM_CONFIG_COM_DEVICE_MEMAP_ADDRESS
        comPortDevice.armCoreSightComponent.memAp = &comPortDeviceMemAp;
#else
        comPortDevice.armCoreSightComponent.memAp = NULL;
#endif
    }

    mExtComPortDriver.reset(new ExternalComPortDriver(comPortDevice, params->debugArchitecture, params->callbacks->registerAccess, params->callbacks->resetStart, params->callbacks->resetFinish, params->refcon));
    if (mExtComPortDriver == 0)
    {
        return SDMReturnCode_InternalError;
    }

    // initialize mbedtools psa crypto api
    if (psa_adac_init() < 0)
    {
        return SDMReturnCode_InternalError;
    } 

    mSdmOpenParams.version = params->version;
    mSdmOpenParams.debugArchitecture = params->debugArchitecture;
    mSdmOpenParams.callbacks = params->callbacks;
    mSdmOpenParams.refcon = params->refcon;
    mSdmOpenParams.resourcesDirectoryPath = params->resourcesDirectoryPath;
    mSdmOpenParams.manifestFilePath = params->manifestFilePath;
    mSdmOpenParams.flags = params->flags;
    mSdmOpenParams.locales = params->locales;
    mSdmOpenParams.connectMode = params->connectMode;

    // SDMOpen calls the EComPort_Init.
    // Upon fail, exit with the fail code.
    uint8_t idResBuff[SD_RESPONSE_LENGTH];
    SDMReturnCode res  = mExtComPortDriver->EComPort_Init(SDM_CONFIG_REMOTE_RESET_TYPE, idResBuff, SD_RESPONSE_LENGTH);
    if (res != SDMReturnCode_Success)
    {
        PSA_ADAC_LOG_ERR(ENTITY_NAME, "EComPort_Init failed [0x%04x]\n", res);
        return res;
    }

    // Upon success the IDResponseBuffer will hold the IDA response of the remote platform (6 bytes).
    // This is used to detect what high level protocol the remote system supports.
    // Verify the IDA response value to be as expected,
    // otherwise fail and return with a proper error code (unsupported remote platform protocol ID).
    res = CheckProtocol(idResBuff, PROTOCOL);
    if (res != SDMReturnCode_Success)
    {
        return res;
    }

    mOpen = true;

    return SDMReturnCode_Success;
}

SDMReturnCode SecureDebugManagerImpl::SDMAuthenticate(const SDMAuthenticateParameters *params)
{
    SDMReturnCode res = SDMReturnCode_Success;

    if (!mOpen)
    {
        return SDMReturnCode_InternalError;
    }

    // load private key and trust chain
    updateProgress("Loading credentials", 0);

    std::unique_ptr<uint8_t> chain;
    size_t chainSize = 0;
    uint8_t signature_type = 0;
    psa_key_handle_t handle = 0;
    res = loadCredentials(chain, chainSize, signature_type, handle);
    if (res != SDMReturnCode_Success)
    {
        return SDMReturnCode_InternalError;
    }

    // start authentication
    updateProgress("Sending challenge request", 20);

    res = sendAuthStartCmdRequest();
    if (res != SDMReturnCode_Success)
    {
        return SDMReturnCode_InternalError;
    }

    // receive challenge
    updateProgress("Receiving challenge", 30);

    psa_auth_challenge_t challenge;
    res = receiveAuthStartCmdResponse(&challenge);
    if (res != SDMReturnCode_Success)
    {
        return SDMReturnCode_InternalError;
    }

    // sign token
    updateProgress("Signing token", 40);

    size_t tokenSize = 0;
    uint8_t *token = 0;
    int adac_res = psa_adac_sign_token(challenge.challenge_vector, sizeof(challenge.challenge_vector), signature_type, NULL, 0, &token, &tokenSize, NULL, handle, NULL, 0);
    if (adac_res < 0)
    {
        PSA_ADAC_LOG_ERR(ENTITY_NAME, "Error signing token %d\n", adac_res);
        return SDMReturnCode_InternalError;
    }

    // parse trust chain
    updateProgress("Parsing trust chain", 50);

    psa_tlv_t *exts[MAX_EXTENSIONS];
    size_t exts_count = 0;
    adac_res = split_tlv_static((uint32_t *)chain.get(), chainSize, exts, MAX_EXTENSIONS, &exts_count);
    if (adac_res < 0)
    {
        PSA_ADAC_LOG_ERR(ENTITY_NAME, "Error parsing trust chain %d\n", adac_res);
        return SDMReturnCode_InternalError;
    }

    PSA_ADAC_LOG_INFO(ENTITY_NAME, "Found %zu certificates\n", exts_count);

    // sending challenge response
    updateProgress("Sending challenge response", 60);

    for (size_t i = 0; i < exts_count; i++) 
    {
        if (exts[i]->type_id == PSA_BINARY_CRT)
        {
            // sending certificate
            res = sendAuthResponseCmdRequest((uint8_t *)exts[i], exts[i]->length_in_bytes + sizeof(psa_tlv_t));
            if (res != SDMReturnCode_Success)
            {
                return SDMReturnCode_InternalError;
            }

            // receiving authentication response
            res = receiveAuthResponseCmdResponse();
            if (res != SDMReturnCode_Success)
            {
                return SDMReturnCode_InternalError;
            }
        }
    }

    // receiving token_authentication response
    updateProgress("Receiving token authentication status", 90);

    res = sendAuthResponseCmdRequest((uint8_t *)token, tokenSize);
    if (res != SDMReturnCode_Success)
    {
        return SDMReturnCode_InternalError;
    }

    res = receiveAuthResponseCmdResponse();
    if (res != SDMReturnCode_Success)
    {
        return SDMReturnCode_InternalError;
    }

    // authentication finished
    updateProgress("Finished authentication", 100);

    return SDMReturnCode_Success;
}

SDMReturnCode SecureDebugManagerImpl::SDMResumeBoot()
{
    return SDMReturnCode_Success;
}

SDMReturnCode SecureDebugManagerImpl::SDMClose()
{
    SDMReturnCode res = SDMReturnCode_Success;

    if (!mOpen)
    {
        return SDMReturnCode_InternalError;
    }

#if SDM_CONFIG_LOCK_ON_CLOSE == true
    // FUTURE: Send to the debugged system 'Lock Debug' command to securely
    // close the debug session. It will not work with CryptoCell-312 in many platforms where the
    // ROM locks DCUs at ROM exit. However, in CryptoIsland-300 it can work. In non CryptoIsland-300
    // platforms, disabling of the debug ports when the ROM locks the DCUs can be implemented
    // by calling SDMClose with remote reset. In this case the DCUs will return to
    // their default values and ROM exit will lock the DCUs.

    SDMReturnCode tmpRes = mExtComPortDriver->EComPort_Finalize();
    if (tmpRes != SDMReturnCode_Success)
    {
        PSA_ADAC_LOG_ERR(ENTITY_NAME, "EComPort_Finalize failed [0x%04x]\n", tmpRes);
        res = tmpRes;
    }
#endif

#if SDM_CONFIG_RESET_ON_CLOSE == true
    if (res == SDMReturnCode_Success && SDM_CONFIG_REMOTE_RESET_TYPE == ECPD_REMOTE_RESET_COM)
    {
        SDMReturnCode tmpRes = mExtComPortDriver->EComPort_RReboot();
        if (tmpRes != SDMReturnCode_Success)
        {
            PSA_ADAC_LOG_ERR(ENTITY_NAME, "EComPortRReboot failed [0x%04x]\n", tmpRes);
            res = tmpRes;
        }
    }
#endif

    mOpen = false;
    return res;
}

/******************************************************************************************************
 *
 * private
 *
 ******************************************************************************************************/

SDMReturnCode SecureDebugManagerImpl::requestPacketSend(request_packet_t *packet)
{
    if (packet == 0)
    {
        return SDMReturnCode_InternalError;
    }

    size_t size = sizeof(request_packet_t) + sizeof(uint32_t) * packet->data_count;
    size_t actual_size = 0;

    SDMReturnCode res = mExtComPortDriver->EComPort_Tx((uint8_t *)packet, size, &actual_size, SDM_CONFIG_COM_HW_TX_BLOCKING);
    if (res != SDMReturnCode_Success)
    {
        PSA_ADAC_LOG_ERR(ENTITY_NAME, "Request packet sendfailed\n");
    }

    return res;
}

SDMReturnCode SecureDebugManagerImpl::responsePacketReceive(response_packet_t *packet, size_t max)
{
    if (packet == 0 || max == 0)
    {
        return SDMReturnCode_InternalError;
    }

    size_t length = 0;

    SDMReturnCode res = mExtComPortDriver->EComPort_Rx((uint8_t *)packet, max, &length);
    if (res != SDMReturnCode_Success)
    {
        PSA_ADAC_LOG_ERR(ENTITY_NAME, "Response packet receive failed\n");
    }

    return res;
}

SDMReturnCode SecureDebugManagerImpl::loadCredentials(std::unique_ptr<uint8_t>& chain, size_t& chain_size, uint8_t& signature_type, psa_key_handle_t& handle)
{
    if (mSdmOpenParams.callbacks->presentForm == 0)
    {
        return SDMReturnCode_InternalError;
    }

    char keyFile[FILENAME_MAX];
    char chainFile[FILENAME_MAX];

    SDMFormElement key_file_element;
    key_file_element.id = "key_file";
    key_file_element.title = "Please provide private key file path: ";
    key_file_element.help = "";
    key_file_element.fieldType = SDMForm_PathSelect;
    key_file_element.flags = 0;
    key_file_element.pathSelect.extensions = 0;
    key_file_element.pathSelect.extensionsCount = 0;
    key_file_element.pathSelect.pathBuffer = keyFile;
    key_file_element.pathSelect.pathBufferLength = FILENAME_MAX;

    SDMFormElement trust_chain_file_element;
    trust_chain_file_element.id = "trust_chain_file";
    trust_chain_file_element.title = "Please provide trust chain file path: ";
    trust_chain_file_element.help = "";
    trust_chain_file_element.fieldType = SDMForm_PathSelect;
    trust_chain_file_element.flags = 0;
    trust_chain_file_element.pathSelect.extensions = 0;
    trust_chain_file_element.pathSelect.extensionsCount = 0;
    trust_chain_file_element.pathSelect.pathBuffer = chainFile;
    trust_chain_file_element.pathSelect.pathBufferLength = FILENAME_MAX;

    SDMFormElement const * elements[] = { &key_file_element, &trust_chain_file_element };

    SDMForm credentials_form;
    credentials_form.id = "credentials_form";
    credentials_form.title = "Credentials form";
    credentials_form.info = 0;
    credentials_form.flags = 0;
    credentials_form.elements = elements;
    credentials_form.elementCount = 2;

    SDMReturnCode res = mSdmOpenParams.callbacks->presentForm(&credentials_form, mSdmOpenParams.refcon);
    if (res != SDMReturnCode_Success)
    {
        return res;
    }

    std::string keyFileStr = userInputStringTrim(keyFile);
    if (import_private_key(keyFileStr.c_str(), &signature_type, &handle) != 0)
    {
        PSA_ADAC_LOG_ERR(ENTITY_NAME, "import_private_key failed\n");
        return SDMReturnCode_InternalError;
    }

    uint8_t* tmpChain = 0;
    std::string chainFileStr = userInputStringTrim(chainFile);
    if (load_trust_chain(chainFileStr.c_str(), &tmpChain, &chain_size) != 0)
    {
        PSA_ADAC_LOG_ERR(ENTITY_NAME, "load_trust_chain failed\n");
        return SDMReturnCode_InternalError;
    }
    chain.reset(tmpChain);

    return SDMReturnCode_Success;
}

SDMReturnCode SecureDebugManagerImpl::sendAuthStartCmdRequest()
{
    request_packet_t request;
    request.command = ADAC_AUTH_START_CMD;
    request.data_count = 0;

    return requestPacketSend(&request);
}

SDMReturnCode SecureDebugManagerImpl::receiveAuthStartCmdResponse(psa_auth_challenge_t *challenge)
{
    response_packet_t *response = (response_packet_t *) mMsgBuffer.data();
    static const size_t max = mMsgBuffer.size() - sizeof(response_packet_t);

    SDMReturnCode res = responsePacketReceive(response, max);
    if (res != SDMReturnCode_Success)
    {
        return res;
    }

    if (!response || response->data_count * 4 != sizeof(psa_auth_challenge_t))
    {
        // invalid response
        challenge = 0;
        return SDMReturnCode_InternalError;
    }
    else
    { 
        if (memcpy(challenge, response->data, sizeof(psa_auth_challenge_t)) == 0)
        {
            return SDMReturnCode_InternalError;
        }
    }

    return SDMReturnCode_Success;
}

SDMReturnCode SecureDebugManagerImpl::sendAuthResponseCmdRequest(uint8_t *cert, size_t certLength)
{
    request_packet_t *request = (request_packet_t *) mMsgBuffer.data();
    request->command = ADAC_AUTH_RESPONSE_CMD;
    request->data_count = certLength / sizeof(uint32_t);
    memcpy((void *) request->data, (void *) cert, certLength);

    return requestPacketSend(request);
}

SDMReturnCode SecureDebugManagerImpl::receiveAuthResponseCmdResponse()
{
    response_packet_t *response = (response_packet_t *) mMsgBuffer.data();
    static const size_t max = mMsgBuffer.size() - sizeof(response_packet_t);

    SDMReturnCode res = responsePacketReceive(response, max);
    if (res != SDMReturnCode_Success)
    {
        return res;
    }

    if (!response)
    {
        return SDMReturnCode_InternalError;
    }
    else if (response->status != ADAC_SUCCESS && response->status != ADAC_NEED_MORE_DATA)
    {
        PSA_ADAC_LOG_ERR(ENTITY_NAME, "Unexpected Auth Response Command response status %x\n", response->status);
        return SDMReturnCode_InternalError;
    }

    return SDMReturnCode_Success;
}

void SecureDebugManagerImpl::updateProgress(const char *progressMessage, uint8_t percentComplete)
{
    if (progressMessage != 0 && mSdmOpenParams.callbacks->updateProgress != 0)
    {
        mSdmOpenParams.callbacks->updateProgress(progressMessage, percentComplete, mSdmOpenParams.refcon);
    }
}
