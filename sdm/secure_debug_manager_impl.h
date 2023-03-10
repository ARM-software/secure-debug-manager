// secure_debug_manager_impl.h
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#ifndef SECURE_DEBUG_MANAGER_IMPL_H
#define SECURE_DEBUG_MANAGER_IMPL_H

#include <memory.h>
#include <vector>

#include "ext_com_port_driver.h"
#include "psa_adac.h"

#define BUFFER_SIZE 4096

class SecureDebugManagerImpl
{
public:

    SecureDebugManagerImpl();
    ~SecureDebugManagerImpl();

    SDMReturnCode SDMOpen(const SDMOpenParameters* params);
    SDMReturnCode SDMAuthenticate(const SDMAuthenticateParameters *params);
    SDMReturnCode SDMResumeBoot();
    SDMReturnCode SDMClose();

private:

    SDMReturnCode requestPacketSend(request_packet_t *packet);
    SDMReturnCode responsePacketReceive(response_packet_t *packet, size_t max);
    SDMReturnCode loadCredentials(std::unique_ptr<uint8_t>& chain, size_t& chain_size, uint8_t& signature_type, psa_key_handle_t& handle);
    
    SDMReturnCode sendAuthStartCmdRequest();
    SDMReturnCode receiveAuthStartCmdResponse(psa_auth_challenge_t *challenge);
    SDMReturnCode sendAuthResponseCmdRequest(uint8_t *ext, size_t extLength);
    SDMReturnCode receiveAuthResponseCmdResponse();

    void updateProgress(const char *progressMessage, uint8_t percentComplete);

    std::vector<uint8_t> mMsgBuffer = std::vector<uint8_t>(BUFFER_SIZE, 0);

    SDMOpenParameters mSdmOpenParams;

    std::unique_ptr<ExternalComPortDriver> mExtComPortDriver;

    bool mInitialized;
    bool mOpen;
};

#endif //SECURE_DEBUG_MANAGER_IMPL_H
