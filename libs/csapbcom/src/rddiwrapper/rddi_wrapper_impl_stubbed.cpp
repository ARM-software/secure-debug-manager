// rddi_wrapper_impl_stubbed.cpp
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#include "rddi_wrapper_impl.h"

int RDDINativeWrapperImplementation::Open(RDDIHandle* handle, const void*)
{
    // Stub (very) basic functionality - returning a new handle
    static int handleCount = 0;

    *handle = ++handleCount;

    return RDDI_SUCCESS;
}

int RDDINativeWrapperImplementation::Close(RDDIHandle)
{
    return RDDI_SUCCESS;
}

int RDDINativeWrapperImplementation::ConfigInfo_OpenFileAndRetarget(RDDIHandle,
            const char*,
            const char*)
{
    return RDDI_SUCCESS;
}

int RDDINativeWrapperImplementation::Debug_Connect(RDDIHandle,
            const char*,
            char*, size_t,
            char*, size_t,
            char*, size_t)
{
    return RDDI_SUCCESS;
}

int RDDINativeWrapperImplementation::Debug_Disconnect(RDDIHandle,
            int)
{
    return RDDI_SUCCESS;
}

int RDDINativeWrapperImplementation::Debug_OpenConn(RDDIHandle,
            int,
            int*,
            int*,
            char*, size_t)
{
    return RDDI_SUCCESS;
}

int RDDINativeWrapperImplementation::Debug_CloseConn(RDDIHandle,
            int)
{
    return RDDI_SUCCESS;
}

int RDDINativeWrapperImplementation::Debug_SetConfig(RDDIHandle,
    int,
    char*,
    char*)
{
    return RDDI_SUCCESS;
}

int RDDINativeWrapperImplementation::Debug_RegReadBlock(RDDIHandle,
            int,
            uint32,
            int,
            uint32*, size_t)
{
    return RDDI_SUCCESS;
}

int RDDINativeWrapperImplementation::Debug_RegWriteBlock(RDDIHandle,
            int,
            uint32,
            int,
            const uint32*)
{
    return RDDI_SUCCESS;
}

int RDDINativeWrapperImplementation::Debug_RegRWList(RDDIHandle,
    int,
    RDDI_REG_ACC_OP*,
    size_t)
{
    return RDDI_SUCCESS;
}

int RDDINativeWrapperImplementation::Debug_SystemReset(RDDIHandle,
            int,
            int)
{
    return RDDI_SUCCESS;
}
