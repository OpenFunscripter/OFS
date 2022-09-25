///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#ifndef EAMAIN_INTERNAL_NETWORKCHANNEL_H
#define EAMAIN_INTERNAL_NETWORKCHANNEL_H

#include <EAMain/internal/EAMainChannels.h>

namespace EA
{
    namespace EAMain
    {
        namespace Internal
        {
            IChannel *CreateNetworkChannel(const char *connection, int port);
        }
    }
}

#endif
