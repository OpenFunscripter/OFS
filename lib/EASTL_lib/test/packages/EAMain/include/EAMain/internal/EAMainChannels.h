///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#ifndef EAMAIN_ICHANNEL_H
#define EAMAIN_ICHANNEL_H

#include <EABase/eabase.h>
#include <EAMain/internal/Version.h>

EA_DISABLE_ALL_VC_WARNINGS()
#include <stdio.h>
EA_RESTORE_ALL_VC_WARNINGS()

namespace EA
{
namespace EAMain
{
    // -----------------------------------------------------------
    // Interface for EAMain channels
    // -----------------------------------------------------------
    class EAMAIN_API IChannel
    {
    public:
        virtual ~IChannel() {}
        virtual void Init() {}
        virtual void Send(const char8_t* pData) {}
        virtual void Shutdown() {}
    };

    // -----------------------------------------------------------
    // Basic channel that echos to stdout.
    // -----------------------------------------------------------
    class EAMAIN_API PrintfChannel : public IChannel
    {
    public:
        virtual ~PrintfChannel() {}
        virtual void Send(const char8_t* pData);
    };

    // -----------------------------------------------------------
    // Channel that serializes output data to a file.
    // -----------------------------------------------------------
    class EAMAIN_API FileChannel : public IChannel
    {
    public:
        virtual ~FileChannel() {}
        virtual void Init();
        virtual void Send(const char8_t* pData);
        virtual void Shutdown();

    private:
        FILE* mFileHandle;
    };
}}

#endif
