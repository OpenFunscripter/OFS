///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#include <EAMain/EAMainExit.h>
#include <EAMain/EAMain.h>

#include <EAStdC/EAMemory.h>

#include <EABase/eabase.h>

EA_DISABLE_ALL_VC_WARNINGS()

#include <cstdlib>

#if defined(EA_PLATFORM_SONY) || defined(EA_PLATFORM_ANDROID)
    // All of these platforms require complex handling of exceptions and signals. No apparent solution yet.
#elif defined(EA_PLATFORM_LINUX)
    #include <sys/signal.h>
#elif defined(EA_PLATFORM_APPLE) || defined(EA_PLATFORM_WINDOWS) || defined(EA_PLATFORM_XBOXONE) || defined(EA_PLATFORM_CAPILANO) || defined(CS_UNDEFINED_STRING)
    #include <csignal>
#endif

EA_RESTORE_ALL_VC_WARNINGS()

#include <EAMain/internal/EAMainStartupShutdown.h>

namespace EA {
    namespace EAMain {
        const char* gExitCodeNames[ExitCode_Max] =
        {
            "Succeeded",
            "Asserted",
            "Abort Signal",
            "Segmentation Fault Signal",
            "Illegal Instruction Signal",
            "Hangup Signal",
            "Floating Point Exception Signal",
            "BusError Signal",
            "Unkown",
        };

        void Exit(int exitcode)
        {
            int index = (exitcode < 0 || exitcode >= ExitCode_Max) ? exitcode = ExitCode_Unknown : exitcode;
            Report("======================================================================\nEA::Main::Exit called with exitcode %d (%s)!\nThe caller wanted to immediately end execution!\n======================================================================\n", exitcode, gExitCodeNames[index]);
#if !defined(EA_PLATFORM_ANDROID)
            Internal::EAMainShutdown(exitcode);
            std::exit(exitcode);
#else
            PlatformShutdown(exitcode);
#endif
        }

#if defined(EA_PLATFORM_SONY) || defined(EA_PLATFORM_ANDROID) || defined(CS_UNDEFINED_STRING)
        void InitializeSignalHandler() {
            // Do nothing. No solution for signal/exception handling on this platform yet.
        }

#elif defined(EA_PLATFORM_WINDOWS) || defined(EA_PLATFORM_XBOXONE) || defined(EA_PLATFORM_CAPILANO) || defined(CS_UNDEFINED_STRING)
        void InitializeSignalHandler()
        {
            // Do nothing. Unhandled exception filter will handle creating a minidump on
            // this platform which will contain more actionable information than a trapped
            // signal.
        }

#elif defined(EA_PLATFORM_APPLE) || defined(EA_PLATFORM_IPHONE)
        int SignalToExitCode(int signal) {
            switch(signal)
            {
            case SIGABRT:
                return ExitCode_SignalAbort;
            case SIGSEGV:
                return ExitCode_SignalSegmentationViolation;
            case SIGILL:
                return ExitCode_SignalIllegalInstruction;
            case SIGFPE:
                return ExitCode_SignalFloatingPointException;
            case SIGHUP:
                return ExitCode_SignalHangup;
            case SIGBUS:
                return ExitCode_SignalBusError;
            default:
                return ExitCode_Unknown;
            };
        }

        void HandleSignal(int signal) {
            EA::EAMain::Exit(SignalToExitCode(signal));
        }

        void InitializeSignalHandler() {
            std::signal(SIGABRT, HandleSignal);
            std::signal(SIGSEGV, HandleSignal);
            std::signal(SIGILL, HandleSignal);
            std::signal(SIGFPE, HandleSignal);
            std::signal(SIGHUP, HandleSignal);
            std::signal(SIGBUS, HandleSignal);
        }
#elif defined(EA_PLATFORM_LINUX)
        int SignalToExitCode(int signal) {
            switch(signal)
            {
            case SIGABRT:
                return ExitCode_SignalAbort;
            case SIGSEGV:
                return ExitCode_SignalSegmentationViolation;
            case SIGILL:
                return ExitCode_SignalIllegalInstruction;
            case SIGHUP:
                return ExitCode_SignalHangup;
            case SIGFPE:
                return ExitCode_SignalFloatingPointException;
            case SIGBUS:
                return ExitCode_SignalBusError;
            default:
                return ExitCode_Unknown;
            };
        }

        struct sigaction ABRTAction;
        struct sigaction SEGVAction;
        struct sigaction SIGILLAction;
        struct sigaction SIGHUPAction;
        struct sigaction SIGFPEAction;
        struct sigaction SIGBUSAction;

        void HandleSignal(int signal, siginfo_t *sigInfo, void *context) {
            EA::EAMain::Exit(SignalToExitCode(signal));
        }

        void InitializeSignalHandler() {
            ABRTAction.sa_sigaction = HandleSignal;
            ABRTAction.sa_flags = SA_SIGINFO;
            SEGVAction.sa_sigaction = HandleSignal;
            SEGVAction.sa_flags = SA_SIGINFO;
            SIGILLAction.sa_sigaction = HandleSignal;
            SIGILLAction.sa_flags = SA_SIGINFO;
            SIGHUPAction.sa_sigaction = HandleSignal;
            SIGHUPAction.sa_flags = SA_SIGINFO;
            SIGFPEAction.sa_sigaction = HandleSignal;
            SIGFPEAction.sa_flags = SA_SIGINFO;
            SIGBUSAction.sa_sigaction = HandleSignal;
            SIGBUSAction.sa_flags = SA_SIGINFO;

            sigaction(SIGABRT, &ABRTAction, NULL);
            sigaction(SIGSEGV, &SEGVAction, NULL);
            sigaction(SIGILL, &SIGILLAction, NULL);
            sigaction(SIGHUP, &SIGHUPAction, NULL);
            sigaction(SIGFPE, &SIGFPEAction, NULL);
            sigaction(SIGBUS, &SIGBUSAction, NULL);
        }
#endif
    }
}
