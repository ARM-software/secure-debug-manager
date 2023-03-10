// secure_debug_manager.cpp
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#include <stdlib.h>
#include <string.h>
#include <memory>

#include "secure_debug_manager.h"
#include "secure_debug_manager_impl.h"

namespace
{
    std::unique_ptr<SecureDebugManagerImpl> gSDMImpl = 0;
}

SDMReturnCode SDMOpen(SDMHandle *handle, const SDMOpenParameters* params)
{
    if (params == 0)
    {
        return SDMReturnCode_InvalidArgument;
    }

    if (gSDMImpl != 0)
    {
        //SDM already open
        return SDMReturnCode_InternalError;
    }

    gSDMImpl.reset(new SecureDebugManagerImpl());
    if (gSDMImpl == 0)
    {
        return SDMReturnCode_InternalError;
    }

    SDMReturnCode ret = gSDMImpl->SDMOpen(params);
    if (ret != SDMReturnCode_Success)
    {
        gSDMImpl.reset();
        return ret;
    }

    *handle = (SDMHandle)gSDMImpl.get();

    return ret;
}

SDMReturnCode SDMAuthenticate(SDMHandle handle, const SDMAuthenticateParameters *params)
{
    if (gSDMImpl == 0)
    {
        // SDM not open
        return SDMReturnCode_InternalError;
    }

    if ((SDMHandle)handle != (SDMHandle)gSDMImpl.get())
    {
        // invalid handle
        return SDMReturnCode_InvalidArgument;
    }

    return gSDMImpl->SDMAuthenticate(params);
}

SDMReturnCode SDMResumeBoot(SDMHandle handle)
{
    if (gSDMImpl == 0)
    {
        // SDM not open
        return SDMReturnCode_InternalError;
    }

    if ((SDMHandle)handle != (SDMHandle)gSDMImpl.get())
    {
        // invalid handle
        return SDMReturnCode_InvalidArgument;
    }

    return gSDMImpl->SDMResumeBoot();
}


SDMReturnCode SDMClose(SDMHandle handle)
{
    if (gSDMImpl == 0)
    {
        // SDM not open
        return SDMReturnCode_InternalError;
    }

    if ((SDMHandle)handle != (SDMHandle)gSDMImpl.get())
    {
        // invalid handle
        return SDMReturnCode_InvalidArgument;
    }

    SDMReturnCode res = gSDMImpl->SDMClose();
    gSDMImpl.reset();

    return res;
}
