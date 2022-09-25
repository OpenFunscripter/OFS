///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#include <kernel.h>
#include <sceerror.h>
#include <gnm.h>
#include "EAAssert/eaassert.h"

#define SUBMIT_DEBUG_PRINT(arg)

namespace EA
{
namespace EAMain
{
namespace Internal
{
    static ScePthread gSubmitDoneThread;
    static SceKernelSema gSubmitDoneSema;
    volatile static bool gShutdownSubmitDoneThread = false;

    void* SubmitDoneThreadFunction(void *)
    {
        int result;
        EA_UNUSED(result);

        SUBMIT_DEBUG_PRINT("Started submit done thread\n");
        for(;;)
        {
            const int microsecondsPerSecond = 1000000;
            SceKernelUseconds timeout = 2 * microsecondsPerSecond;
            result = sceKernelWaitSema(gSubmitDoneSema, 1, &timeout);
            EA_ASSERT(result == SCE_OK || result == SCE_KERNEL_ERROR_ETIMEDOUT);

            if(gShutdownSubmitDoneThread)
            {
                // Break out to avoid calling submitDone
                break;
            }
            else
            {
               SUBMIT_DEBUG_PRINT("submitDone\n");

#if SCE_ORBIS_SDK_VERSION >= 0x01000051u && SCE_ORBIS_SDK_VERSION != 0x01000071u
               // Only perform the submitDone call if we are above SDK version 1.00.051
               // but not 1.00.071 since the requirement was temporarily removed in that release.
               // This is not required in previous or later SDK versions.
               sce::Gnm::submitDone();
#endif
            }
        }
        SUBMIT_DEBUG_PRINT("Ending submit done thread\n");
        return nullptr;
    }

    void StartSubmitDoneThread()
    {
        int result;
        EA_UNUSED(result);

        SUBMIT_DEBUG_PRINT("Starting submit done thread\n");

        result = sceKernelCreateSema(&gSubmitDoneSema, "submit done semaphore", 0, 0, 1, nullptr);
        EA_ASSERT(result == SCE_OK);

        result  = scePthreadCreate(&gSubmitDoneThread, NULL, SubmitDoneThreadFunction, NULL, "submit done thread");
        EA_ASSERT(result == SCE_OK);
    }

    void ShutdownSubmitDoneThread()
    {
        if(!gShutdownSubmitDoneThread)
        {
            SUBMIT_DEBUG_PRINT("Disabling submit done thread\n");
            int result;
            EA_UNUSED(result);

            // Indicate that the submit done thread should exit
            gShutdownSubmitDoneThread = true;

            // Signal semaphore to unblock the submit done thread
            result = sceKernelSignalSema(gSubmitDoneSema, 1);
            EA_ASSERT(result == SCE_OK);

            // Wait for the thread to exit
            result = scePthreadJoin(gSubmitDoneThread, nullptr);
            EA_ASSERT(result == SCE_OK);

            // Free up kernel resources
            result = sceKernelDeleteSema(gSubmitDoneSema);
            EA_ASSERT(result == SCE_OK);
        }
    }
}


void DisableSubmitDoneThread()
{
    using namespace EA::EAMain::Internal;

    ShutdownSubmitDoneThread();
}


}
}

#undef SUBMIT_DEBUG_PRINT
