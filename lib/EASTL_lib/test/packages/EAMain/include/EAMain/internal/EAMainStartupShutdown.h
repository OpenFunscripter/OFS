///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#ifndef EAMAIN_EAENTRYPOINTMAIN_INTERNAL_STARTUPSHUTDOWN_H
#define EAMAIN_EAENTRYPOINTMAIN_INTERNAL_STARTUPSHUTDOWN_H

#include <EABase/eabase.h>
#include <EAMain/internal/EAMainPrintManager.h>

EA_DISABLE_ALL_VC_WARNINGS()
#include <stdio.h>
EA_RESTORE_ALL_VC_WARNINGS()

namespace EA
{
    namespace EAMain
    {
        namespace Internal
        {
            EAMAIN_API void EAMainStartup(const char* printServerAddress = NULL);
            EAMAIN_API int EAMainShutdown(int errorCount);
        }
    }
}

#endif // header include guard
