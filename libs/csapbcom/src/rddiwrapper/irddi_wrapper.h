// irddi_wrapper.h
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#ifndef IRDDI_WRAPPER_H
#define IRDDI_WRAPPER_H

#include "rddi.h"
#include "rddi_debug.h"

class IRDDIWrapper
{
public:
    virtual ~IRDDIWrapper() {}

    virtual int Open(RDDIHandle* handle,
            const void* details) = 0;

    virtual int Close(RDDIHandle handle) = 0;

    virtual int ConfigInfo_OpenFileAndRetarget(RDDIHandle handle,
            const char* filename,
            const char* address) = 0;

    virtual int Debug_Connect(RDDIHandle handle,
            const char* userName,
            char* clientInfo, size_t clientInfoLength,
            char* iceInfo, size_t iceInfoLength,
            char* copyrightInfo, size_t copyrightInfoLength) = 0;

    virtual int Debug_Disconnect(RDDIHandle handle,
            int termAll) = 0;

    virtual int Debug_OpenConn(RDDIHandle handle,
            int deviceNo,
            int* id,
            int* version,
            char* message, size_t messageLength) = 0;

    virtual int Debug_CloseConn(RDDIHandle handle,
            int deviceNo) = 0;

    virtual int Debug_SetConfig(RDDIHandle handle,
            int deviceNo,
            char* configName,
            char* configValue) = 0;

    virtual int Debug_RegReadBlock(RDDIHandle handle,
            int deviceNo,
            uint32 startID,
            int count,
            uint32* values, size_t valuesLength) = 0;

    virtual int Debug_RegWriteBlock(RDDIHandle handle,
            int deviceNo,
            uint32 startID,
            int count,
            const uint32* values) = 0;

    virtual int Debug_RegRWList(RDDIHandle handle,
            int deviceNo,
            RDDI_REG_ACC_OP* pRegisterAccList,
            size_t  registerAccListLength) = 0;

    virtual int Debug_SystemReset(RDDIHandle handle,
            int deviceNo,
            int resetType) = 0;
};

#endif // IRDDI_WRAPPER_H
