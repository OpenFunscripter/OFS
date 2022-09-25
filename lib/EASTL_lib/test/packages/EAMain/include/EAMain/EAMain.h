///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Provides a print server for retrieving console output and a simple
// command line parser.
/////////////////////////////////////////////////////////////////////////////

#ifndef EAMAIN_H
#define EAMAIN_H

#ifdef _MSC_VER
    #pragma warning(push, 0)        // Microsoft headers generate warnings at our higher warning levels.
    #pragma warning(disable: 4530)  // C++ exception handler used, but unwind semantics are not enabled.
    #pragma warning(disable: 4548)  // Expression before comma has no effect; expected expression with side-effect.
    #pragma warning(disable: 4251)  // class (some template) needs to have dll-interface to be used by clients.
#endif

#include <EABase/eabase.h>
#include <EAMain/internal/Version.h>

namespace EA
{
    namespace EAMain
    {
        typedef int (*EAMainFunction)(int, char **);
    }
}

EA_DISABLE_ALL_VC_WARNINGS()
#include <stddef.h>
#include <limits.h>
#include <stdarg.h>
EA_RESTORE_ALL_VC_WARNINGS()

namespace EA
{
    namespace EAMain
    {
        typedef void (*ReportFunction)(const char8_t*);
        EAMAIN_API void           SetReportFunction(ReportFunction pReportFunction);
        EAMAIN_API ReportFunction GetReportFunction();
        EAMAIN_API ReportFunction GetDefaultReportFunction();

        /// Report
        /// Provides a way to call Report with sprintf-style arguments.
        /// This function will call the Report function after formatting the output.
        /// This function acts just like printf, except that the output goes to the
        /// given report function.
        ///
        /// The user needs to supply a newline if the user wants newlines, as the report
        /// function will not append one. The user may supply multiple newlines if desired.
        /// This is a low level function which user code can use to directly write
        /// information to the debug output. This function is also used by the higher
        /// level functionality here to write output.
        ///
        /// This function is the equivalent of ReportVerbosity(0, pFormat, ...).
        ///
        /// Example usage:
        ///     Report("Time passed: %d\n", timeDelta);
        ///
        EAMAIN_API void Report(const char8_t* pFormat, ...);

        /// ReportVerbosity
        /// Same as Report, but is silent unless GetVerbosity() is >= the value specified as minVerbosity.
        /// Typically to do a non-error trace print, you would specify a minVerbosity of 1.
        /// If you are writing an error output, you can specify minVerbosity or 0, which is the same
        /// as calling Report().
        ///
        /// Example usage:
        ///     ReportVerbosity(1, "Time passed: %d\n", timeDelta);
        ///
        EAMAIN_API void ReportVerbosity(unsigned minVerbosity, const char8_t* pFormat, ...);

        /// VReport
        /// Called by EATest Report Wrapper to preserve the optional variable arguments
        ///
        EAMAIN_API void VReport(const char8_t* pFormat, va_list arguments);

        /// VReportVerbosity
        /// Called by EATest ReportVerbosity Wrapper to preserve the optional variable arguments
        ///
        EAMAIN_API void VReportVerbosity(unsigned minVerbosity, const char8_t* pFormat, va_list arguments);

        ///////////////////////////////////////////////////////////////////////
        /// GetVerbosity / SetVerbosity
        ///
        /// A value of 0 means to output just error results.
        /// A value > 0 means to output more. This Test library doesn't itself
        /// use this verbosity value; it's intended for the application to use
        /// it while tracing test output.
        ///
        EAMAIN_API unsigned GetVerbosity();
        EAMAIN_API void     SetVerbosity(unsigned verbosity);

        /// PlatformStartup / PlatformShutdown
        ///
        /// Execute any necessary per-platform startup / shutdown code.
        /// This should be executed once as the first line of main() and
        /// once as the last line of main().
        ///

        /// DEPRECATED
        /// Please use the PlatformStartup(int argc, char **argv) preferably
        EAMAIN_API void PlatformStartup();

        EAMAIN_API void PlatformStartup(int argc, char **argv);

        EAMAIN_API void PlatformStartup(const char *printServerNetworkAddress);

        EAMAIN_API void PlatformShutdown(int nError);

        /// CommandLine
        ///
        /// Implements a small command line parser.
        ///
        /// Example usage:
        ///     int main(int argc, char** argv) {
        ///         CommandLine cmdLine(argc, argv);
        ///         printf("%d", cmdLine.Argc());
        ///     }
        ///
        class EAMAIN_API CommandLine
        {
        public:
            enum Flags
            {
                FLAG_NONE            = 0,
                FLAG_NO_PROGRAM_NAME = 1 << 0
            };

            static const char DEFAULT_DELIMITER = ':';
            static const int MAX_COMMANDLINE_ARGS = 128;

            CommandLine(int argc, char** argv);
            explicit CommandLine(const char *commandLineString);
            CommandLine(const char *commandLineString, unsigned int flags);
            ~CommandLine();

            int FindSwitch(const char *pSwitch, bool bCaseSensitive = false, const char **pResult = NULL, int nStartingIndex = 0, char delimiter = DEFAULT_DELIMITER) const;
            bool HasHelpSwitch() const;

            int Argc() const
            {
                return mArgc;
            }

            char** Argv() const
            {
                return mArgv;
            }

            const char *Arg(int pos) const
            {
                if (pos <= mArgc)
                {
                    return mArgv[pos];
                }

                return "";
            }

        private:
            void ParseCommandLine(const char *args, unsigned int flags);

            int    mArgc;
            char** mArgv;
            char*  mCommandLine;
        };


    } //namespace EAMain

#if (defined(EA_PLATFORM_MICROSOFT) && !defined(CS_UNDEFINED_STRING) && !EA_WINAPI_FAMILY_PARTITION(EA_WINAPI_PARTITION_DESKTOP))
    namespace EAMain
    {
        // Helper class for spawning tests in a separate thread on WinRT platforms, works around <future> include issues.
        class IWinRTRunner
        {
        public:
            virtual ~IWinRTRunner() {}
            virtual void Run(int argc = 0, char** argv = NULL) = 0;
            virtual bool IsFinished() = 0;
            virtual void ReportResult() = 0;
        };

        EAMAIN_API IWinRTRunner* CreateWinRTRunner();
    }
#elif defined(EA_PLATFORM_KETTLE)
    namespace EAMain
    {
        // By default EAMain will spawn a thread on Kettle which calls Gnm::submitDone to prevent
        // the OS frome terminating the application.
        //
        // Applications that want to use the GPU (and will therefore need to call submitDone themselves)
        // can call DisableSubmitDoneThread() after EAMain() is called.  No calls to submitDone will be made
        // by this thread once DisableSubmitDoneThread() returns.
        //
        // Note: For thread safety, this function must be called from the same thread that EAMain is called from.
        void DisableSubmitDoneThread();
    }
#endif

} // namespace EA

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

#endif // Header include guard
