///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#include <EAMain/EAMain.h>
#include <EAMain/internal/EAMainPrintManager.h>
#include <EAMain/internal/EAMainChannels.h>
#include <internal/NetworkChannel.h>
#include <EAAssert/eaassert.h>
#include <EAStdC/EAString.h>
#include <EAMainPrintf.h>

#include <EABase/eabase.h>

EA_DISABLE_ALL_VC_WARNINGS()

#include <string.h>

EA_RESTORE_ALL_VC_WARNINGS()

namespace EA {
namespace EAMain {

//------------------------------------------------------------
// STATICS
//------------------------------------------------------------
static PrintManager gPrintManager;
static PrintfChannel gPrintfChannel;
static FileChannel gFileChannel;
//------------------------------------------------------------


//------------------------------------------------------------
PrintManager& PrintManager::Instance()
{
    return gPrintManager;
}

//------------------------------------------------------------
PrintManager::PrintManager()
{
    memset(m_Channels, 0, sizeof(m_Channels));
}

//------------------------------------------------------------
void PrintManager::Send(const char8_t* pData)
{
    // Broadcast the message to all the registered channels.
    for(int i = 0; i < CHANNEL_MAX; i++)
    {
        if(m_Channels[i])
            m_Channels[i]->Send(pData);
    }
}

//------------------------------------------------------------
void PrintManager::Add(EAMainChannel channel, IChannel* instance)
{
    EA_ASSERT_MSG(instance, "invalid channel instance");
    EA_ASSERT_MSG(m_Channels[channel] == NULL, "channel already added to the list");

    if(instance != NULL && m_Channels[channel] == NULL)
    {
        // Initialize the channel, then add it to the channel vector.
        instance->Init();

        // Add the channel to the array
        m_Channels[channel] = instance;
    }
}

//------------------------------------------------------------
void PrintManager::ClearChannel(EAMainChannel channel)
{
    if (m_Channels[channel] != NULL)
    {
        IChannel* instance = m_Channels[channel];

        // Shut down the channel.
        instance->Shutdown();

        // Remove the channel from the array.
        m_Channels[channel] = NULL;
    }
}
//------------------------------------------------------------
void PrintManager::Remove(EAMainChannel channel, IChannel* instance)
{
    EA_ASSERT_MSG(instance, "invalid channel instance");
    EA_ASSERT_MSG(m_Channels[channel] != NULL, "channel not added to list yet");

    if(instance != NULL && m_Channels[channel] != NULL)
    {
        // Shut down the channel.
        instance->Shutdown();

        // Remove the channel from the array.
        m_Channels[channel] = NULL;
    }
}

//------------------------------------------------------------
void PrintManager::Startup(const char8_t* printServerAddress)
{
    // Register PrintManager print function with the reporting module.
    //
#if EAMAIN_DISABLE_DEFAULT_NETWORK_CHANNEL
    if (!printServerAddress)
    {
        Add(CHANNEL_PRINTF, &gPrintfChannel);
        return;
    }
#endif
    EA::EAMain::SetReportFunction(EA::EAMain::Messages::Print);
    if (!printServerAddress)
    {
        printServerAddress = MACRO_TO_STRING(EAMAIN_NETWORK_CHANNEL_IP);
    }
    const char8_t* server = printServerAddress;
    const char8_t* portStr = strchr(printServerAddress, ':');

    int port = EAMAIN_NETWORK_CHANNEL_PORT;
    char8_t serverBuff[64] = { 0 };
    if (portStr != NULL)
    {
        port = EA::StdC::AtoI32(portStr + 1);
        server = EA::StdC::Strncpy(serverBuff, server, portStr - server);
    }

    IChannel *networkChannel = Internal::CreateNetworkChannel(server, port);
    if (networkChannel)
    {
        Add(CHANNEL_NETWORK, networkChannel);
    }
    else
    {
        Add(CHANNEL_PRINTF, &gPrintfChannel);
    }
}

//------------------------------------------------------------
void PrintManager::Shutdown()
{
    for(int i = 0; i < CHANNEL_MAX; i++)
    {
        if(m_Channels[i])
        {
            m_Channels[i]->Shutdown();
            m_Channels[i] = NULL;
        }
    }
}

}}
