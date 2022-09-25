///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#ifndef EAMAIN_PRINTMANAGER_H
#define EAMAIN_PRINTMANAGER_H

#include <EABase/eabase.h>
#include <EAMain/internal/Version.h>

#define TO_STRING(x) #x
#define MACRO_TO_STRING(x) TO_STRING(x)

namespace EA
{
namespace EAMain
{
    class IChannel;

    enum EAMainChannel
    {
        CHANNEL_PRINTF = 0,
        CHANNEL_NETWORK,
        CHANNEL_FILE,
        CHANNEL_MAX
    };

    class EAMAIN_API PrintManager
    {
    public:
        PrintManager();
        static PrintManager& Instance();

        void Startup(const char8_t* ServerIP);
        void Shutdown();

        void Send(const char8_t* pData);
        void Add(EAMainChannel channel, IChannel* instance);
        void Remove(EAMainChannel channel, IChannel* instance);
        void ClearChannel(EAMainChannel channel);

    private:
        IChannel* m_Channels[CHANNEL_MAX];
    };
}}

#endif  // header include guard
