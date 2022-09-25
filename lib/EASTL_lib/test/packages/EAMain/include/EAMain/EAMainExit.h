///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Provides an exit function for clients to immediately close the application
// without needing to throw an exception or call abort.
///////////////////////////////////////////////////////////////////////////////

#ifndef EAMAINEXIT_H
#define EAMAINEXIT_H

#ifdef _MSC_VER
    #pragma warning(push, 0)        // Microsoft headers generate warnings at our higher warning levels.
    #pragma warning(disable: 4530)  // C++ exception handler used, but unwind semantics are not enabled.
    #pragma warning(disable: 4548)  // Expression before comma has no effect; expected expression with side-effect.
    #pragma warning(disable: 4251)  // class (some template) needs to have dll-interface to be used by clients.
#endif

#include <EABase/eabase.h>
#include <EAMain/internal/Version.h>

namespace EA {
    namespace EAMain {
        enum ExitCodes {
            ExitCode_Succeeded,
            ExitCode_Asserted,
            ExitCode_SignalAbort,
            ExitCode_SignalSegmentationViolation,
            ExitCode_SignalIllegalInstruction,
            ExitCode_SignalHangup,
            ExitCode_SignalFloatingPointException,
            ExitCode_SignalBusError,
            ExitCode_Unknown,
            ExitCode_Max
        };

        EAMAIN_API EA_NO_INLINE void Exit(int exitcode);

        EAMAIN_API EA_NO_INLINE void InitializeSignalHandler();
    }
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

#endif // Header include guard
