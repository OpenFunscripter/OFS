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

            // Handle the internal EAMain main start-up
            //
            inline void EAMainStartup(const char8_t* ServerIP = NULL)
            {
                static bool sEAMainShutdown_StartupHandled = false;

                if(!sEAMainShutdown_StartupHandled)
                {
                    sEAMainShutdown_StartupHandled = true;

                    // Running under NAnt output only appears when the buffer is filled if we allow the default buffering scheme for printf.
                    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
                    setvbuf(stderr, NULL, _IONBF, BUFSIZ);

                    // Startup the print manager
                    //
                    EA::EAMain::PrintManager::Instance().Startup(ServerIP);
                }
            }


            // Handle the internal EAMain main shutdown
            //
            inline int EAMainShutdown(int errorCount)
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
                    EA::EAMain::Report("RETURNCODE=%d\n", errorCount);

                    // Shutdown the EAMain print manager.
                    //
                    EA::EAMain::PrintManager::Instance().Shutdown();
               }

               return errorCount;
            }
        } //namespace Internal
    } // namespace EAMain
} // namespace EA

#endif // header include guard
