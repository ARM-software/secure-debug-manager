// rddi_wrapper_impl_default.cpp
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#include "rddi_wrapper_impl.h"

#include "rddi.h"
#include "rddi_configinfo.h"
#include "rddi_debug.h"

int RDDINativeWrapperImplementation::Open(RDDIHandle* handle, const void* details)
{
    return ::RDDI_Open(handle, details);
}

int RDDINativeWrapperImplementation::Close(RDDIHandle handle)
{
    return ::RDDI_Close(handle);
}

int RDDINativeWrapperImplementation::ConfigInfo_OpenFileAndRetarget(RDDIHandle handle,
            const char* filename,
            const char* address)
{
    return ::ConfigInfo_OpenFileAndRetarget(handle, filename, address);
}

int RDDINativeWrapperImplementation::Debug_Connect(RDDIHandle handle,
            const char* userName,
            char* clientInfo, size_t clientInfoLength,
            char* iceInfo, size_t iceInfoLength,
            char* copyrightInfo, size_t copyrightInfoLength)
{
    return ::Debug_Connect(handle, userName, clientInfo, clientInfoLength,
            iceInfo, iceInfoLength, copyrightInfo, copyrightInfoLength);
}

int RDDINativeWrapperImplementation::Debug_Disconnect(RDDIHandle handle,
            int termAll)
{
    return ::Debug_Disconnect(handle, termAll);
}

int RDDINativeWrapperImplementation::Debug_OpenConn(RDDIHandle handle,
            int deviceNo,
            int* id,
            int* version,
            char* message, size_t messageLength)
{
    return ::Debug_OpenConn(handle, deviceNo, id, version, message, messageLength);
}

int RDDINativeWrapperImplementation::Debug_CloseConn(RDDIHandle handle,
            int deviceNo)
{
    return ::Debug_CloseConn(handle, deviceNo);
}

int RDDINativeWrapperImplementation::Debug_SetConfig(RDDIHandle handle,
    int deviceNo,
    char* configName,
    char* configValue)
{
    return ::Debug_SetConfig(handle, deviceNo, configName, configValue);
}

int RDDINativeWrapperImplementation::Debug_RegReadBlock(RDDIHandle handle,
            int deviceNo,
            uint32 startID,
            int count,
            uint32* values, size_t valuesLength)
{
    return ::Debug_RegReadBlock(handle, deviceNo, startID, count, values, valuesLength);
}

int RDDINativeWrapperImplementation::Debug_RegWriteBlock(RDDIHandle handle,
            int deviceNo,
            uint32 startID,
            int count,
            const uint32* values)
{
    return ::Debug_RegWriteBlock(handle, deviceNo, startID, count, values);
}

int RDDINativeWrapperImplementation::Debug_RegRWList(RDDIHandle handle,
    int deviceNo,
    RDDI_REG_ACC_OP* pRegisterAccList,
    size_t  registerAccListLength)
{
    return ::Debug_RegRWList(handle, deviceNo, pRegisterAccList, registerAccListLength);
}

int RDDINativeWrapperImplementation::Debug_SystemReset(RDDIHandle handle,
            int deviceNo,
            int resetType)
{
    return ::Debug_SystemReset(handle, deviceNo, resetType);
}
