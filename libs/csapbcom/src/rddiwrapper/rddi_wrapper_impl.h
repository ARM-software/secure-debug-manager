// rddi_wrapper_impl.h
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#ifndef RDDI_WRAPPER_IMPLEMENTATION_H
#define RDDI_WRAPPER_IMPLEMENTATION_H

#include "irddi_wrapper.h"

class RDDINativeWrapperImplementation : public IRDDIWrapper
{
public:
    virtual int Open(RDDIHandle* handle,
            const void* details);

    virtual int Close(RDDIHandle handle);

    virtual int ConfigInfo_OpenFileAndRetarget(RDDIHandle handle,
            const char* filename,
            const char* address);

    virtual int Debug_Connect(RDDIHandle handle,
            const char* userName,
            char* clientInfo, size_t clientInfoLength,
            char* iceInfo, size_t iceInfoLength,
            char* copyrightInfo, size_t copyrightInfoLength);

    virtual int Debug_Disconnect(RDDIHandle handle,
            int termAll);

    virtual int Debug_OpenConn(RDDIHandle handle,
            int deviceNo,
            int* id,
            int* version,
            char* message, size_t messageLength);

    virtual int Debug_CloseConn(RDDIHandle handle,
            int deviceNo);

    virtual int Debug_SetConfig(RDDIHandle handle,
            int deviceNo,
            char* configName,
            char* configValue);

    virtual int Debug_RegReadBlock(RDDIHandle handle,
            int deviceNo,
            uint32 startID,
            int count,
            uint32* values, size_t valuesLength);

    virtual int Debug_RegWriteBlock(RDDIHandle handle,
            int deviceNo,
            uint32 startID,
            int count,
            const uint32* values);

    virtual int Debug_RegRWList(RDDIHandle handle,
            int deviceNo,
            RDDI_REG_ACC_OP* pRegisterAccList,
            size_t 	registerAccListLength);

    virtual int Debug_SystemReset(RDDIHandle handle,
            int deviceNo,
            int resetType);
};
#endif // RDDI_WRAPPER_IMPLEMENTATION_H
