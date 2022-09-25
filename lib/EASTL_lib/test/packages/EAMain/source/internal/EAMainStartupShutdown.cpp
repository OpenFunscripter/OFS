///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#include <EAMain/internal/EAMainStartupShutdown.h>
#include <EAMain/EAMain.h>
#include <EAMain/EAMainExit.h>


#ifdef _MSC_VER
    #pragma warning(push, 0)        // Microsoft headers generate warnings at our higher warning levels.
    #pragma warning(disable: 4702)  // Unreachable code detected.
#endif

#if defined(EA_PLATFORM_MICROSOFT)
    #if defined(EA_PLATFORM_WINDOWS_PHONE) || defined(EA_PLATFORM_WINRT)
        #define EAMAIN_HAVE_UNHANDLED_EXCEPTION_FILTER 0
    #else
        #if !defined(WIN32_LEAN_AND_MEAN)
            #define WIN32_LEAN_AND_MEAN
        #endif
        #if defined(EA_PLATFORM_CAPILANO)
            #include <xdk.h>
        #endif
        EA_DISABLE_ALL_VC_WARNINGS();
        #include <Windows.h>
        #include <DbgHelp.h>
        EA_RESTORE_ALL_VC_WARNINGS();
        #include <EAStdC/EASprintf.h>
        #define EAMAIN_HAVE_UNHANDLED_EXCEPTION_FILTER 1
    #endif
#endif

#if !defined(EAMAIN_HAVE_UNHANDLED_EXCEPTION_FILTER)
    #define EAMAIN_HAVE_UNHANDLED_EXCEPTION_FILTER 0
#endif

namespace EA
{
    namespace EAMain
    {
        namespace Internal
        {
            #if EAMAIN_HAVE_UNHANDLED_EXCEPTION_FILTER
                static LONG WINAPI EAMainUnhandledExceptionFilter(LPEXCEPTION_POINTERS exception)
                {
                    EA::EAMain::Report("\n");
                    EA::EAMain::Report("===============================================================================\n");
                    EA::EAMain::Report("ATTENTION ATTENTION ATTENTION ATTENTION ATTENTION ATTENTION ATTENTION ATTENTION\n");
                    EA::EAMain::Report("An unhandled exception has been detected. This likely means the application is \n");
                    EA::EAMain::Report("crashing.\n\n");
                    EA::EAMain::Report("(This message is courtesy of EAMain but does not mean that EAMain is\n");
                    EA::EAMain::Report("the cause of the crash.)\n");
                    EA::EAMain::Report("===============================================================================\n");

                    #if EAMAIN_MINIDUMP_SUPPORTED
                        char8_t szPath[MAX_PATH];
                        char8_t szFileName[MAX_PATH];
                        HANDLE hDumpFile;
                        SYSTEMTIME stLocalTime;
                        MINIDUMP_EXCEPTION_INFORMATION ExpParam;
                        BOOL bMiniDumpSuccessful;

                        #if defined(EA_PLATFORM_CAPILANO)
                            const char8_t* pszDrive = "G:\\";
                        #else
                            const char8_t* pszDrive = "C:\\";
                        #endif

                        GetLocalTime( &stLocalTime );
                        EA::StdC::Snprintf(szPath, EAArrayCount(szPath), "%sMiniDumps\\", pszDrive);
                        CreateDirectoryA(szPath, NULL );

                        EA::StdC::Snprintf(szFileName, EAArrayCount(szFileName), "%sMiniDump-%04d%02d%02d-%02d%02d%02d-%ld-%ld.dmp",
                                    szPath,
                                    stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay,
                                    stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond,
                                    GetCurrentProcessId(), GetCurrentThreadId());

                        EA::EAMain::Report("Creating Dump File: %s - ", szFileName);
                        hDumpFile = CreateFileA(szFileName, GENERIC_READ|GENERIC_WRITE,
                                    FILE_SHARE_WRITE|FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
                        EA::EAMain::Report("%s (Error: %d) \n", hDumpFile != INVALID_HANDLE_VALUE ? "Success" : "Failure", hDumpFile != INVALID_HANDLE_VALUE ? 0 : HRESULT_FROM_WIN32(GetLastError()));

                        ExpParam.ThreadId = GetCurrentThreadId();
                        ExpParam.ExceptionPointers = exception;
                        ExpParam.ClientPointers = TRUE;
                        bMiniDumpSuccessful = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                                        hDumpFile, MiniDumpWithDataSegs, &ExpParam, NULL, NULL);

                        EA::EAMain::Report("Dump %s (Error: %d)\n", bMiniDumpSuccessful == TRUE ? "Successful" : "Failed - so deleted minidump file", bMiniDumpSuccessful == TRUE ? 0 : HRESULT_FROM_WIN32(GetLastError()));
                        if (bMiniDumpSuccessful == FALSE)
                        {
                            DeleteFileA(szFileName);
                        }

                    #endif

                    EAMainShutdown(1);

                    return EXCEPTION_CONTINUE_SEARCH;
                }
            #endif

            void EAMainStartup(const char8_t* printServerAddress)
            {
                static bool sEAMainShutdown_StartupHandled = false;
                if(!sEAMainShutdown_StartupHandled)
                {
                    sEAMainShutdown_StartupHandled = true;

                    #if EAMAIN_HAVE_UNHANDLED_EXCEPTION_FILTER
                        SetUnhandledExceptionFilter(EAMainUnhandledExceptionFilter);
                    #endif

                    // Running under NAnt output only appears when the buffer is filled if we allow the default buffering scheme for printf.
                    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
                    setvbuf(stderr, NULL, _IONBF, BUFSIZ);

                    // Startup the print manager
                    //
                    EA::EAMain::PrintManager::Instance().Startup(printServerAddress);
                }
            }

            int EAMainShutdown(int errorCount)
            {
               static bool sEAMainShutdown_ShutdownHandled = false;
               if(!sEAMainShutdown_ShutdownHandled)
               {
                    sEAMainShutdown_ShutdownHandled = true;

                    // Handle the application specific exit code.
                    //
                    #if defined(EA_PLATFORM_IPHONE)
                       // Get test result. (iOS 5 bug prevents iPhone Runner from getting this from the exit code)
                       if (errorCount == 0)
                           Report("\nAll tests completed successfully.\n");
                       else
                           Report("\nTests failed. Total error count: %d\n", errorCount);
                       fflush(stdout);
                    #endif

                    #if !defined(EA_PLATFORM_DESKTOP) && !defined(EA_PLATFORM_SERVER)  // TODO:  change define to something related to the StdC library used on the system.
                        fflush(stdout);
                    #endif

                    // Required so the EAMainPrintServer can terminate with the correct error code.
                    //
                    EA::EAMain::Report("\nRETURNCODE=%d\n", errorCount);

                    // Shutdown the EAMain print manager.
                    //
                    EA::EAMain::PrintManager::Instance().Shutdown();
               }

               return errorCount;
            }

        }
    }
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
