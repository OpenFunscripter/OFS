///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#ifndef EAMAIN_EAENTRYPOINTMAIN_INL
#define EAMAIN_EAENTRYPOINTMAIN_INL

#include <EAMain/internal/Version.h>
#include <EAMain/EAMain.h>
#include <EAMain/EAMainExit.h>

#include <EAMain/internal/EAMainStartupShutdown.h>

#if defined(EA_PLATFORM_WINDOWS)
    #include <EABase/eabase.h>

    EA_DISABLE_ALL_VC_WARNINGS()
    #include <EAStdC/EAString.h>
    #include <malloc.h>
    #define NOMINMAX
    #include <Windows.h>
    #undef NOMINMAX
    EA_RESTORE_ALL_VC_WARNINGS()
#endif


/// EAEntryPointMain
///
/// Provides a platform-independent main entry function. The user must implement
/// this function himself. Different platforms have different variations of
/// main and some platforms have multiple variations of main within the platform.
/// EAEnrtyPointMain is a universal entrypoint which you can use to implement a platform-
/// and string encoding- independent main entrypoint.
///
/// You use this function by #including "EATestApp/EAMain.h" and implementing the
/// function. You don't need to use this function and this code is neither seen
/// nor compiled if you don't use it. It is merely a convenience to deal with
/// platform indepdence in a proper way and also a reference for how to do such
/// a thing manually.
///
/// Microsoft-specific:
/// By default an 8 bit C Standard main is implemented unless EAMAIN_MAIN_16 is defined.
/// If EAMAIN_MAIN_16 is defined under a supporting Microsoft platform then either
/// wWinMain or wmain is used. You need to use EAMAIN_MAIN_16 under Microsoft platforms if
/// you want arguments to be in Unicode, as Windows 8 bit arguments are not UTF8 or any
/// kind of Unicode.
///
/// Example usage:
///    #include <EAMain/EAEntryPointMain.inl>
///
///    int EAMain(int argc, char** argv){
///        printf("hello world");
///    }
///
extern "C" int EAMain(int argc, char** argv);

namespace EA
{
    namespace EAMain
    {
        namespace Internal
        {
            EAMAIN_API extern EAMainFunction gEAMainFunction;

            EAMAIN_API const char *ExtractPrintServerAddress(int argc, char **argv);
        }
    }
}


// Windows compilers #define _CONSOLE when the intended target is a console application instead of
// a windows application.
// Note: _CONSOLE doesn't seem to be getting defined automatically (at least for native nant builds).
//       Temporarily define _CONSOLE ourselves to allow the build to work on windows.
#ifndef _CONSOLE
    #define _CONSOLE
#endif

// Windows RT-based applications need to use Microsoft's Managed C++ extensions with their
// Windows::ApplicationModel::Core::IFrameworkView class, as opposed to class C main(argc, argv).
// Additionally, a new kind of main is required with the signature: int main(Platform::Array<Platform::String^>^ args).
// The code below doesn't call EAEntryPointMain directly, but the WinRTRunner that it calls will call EAEntryPointMain.
#if (defined(EA_PLATFORM_MICROSOFT) && !defined(CS_UNDEFINED_STRING) && !EA_WINAPI_FAMILY_PARTITION(EA_WINAPI_PARTITION_DESKTOP))

    // Allow the user to define their own WinRT application entry point should
    // they have a need to create their own IApplicationView.
    //
    // In most cases this shouldn't ever be touched. Likely, if you find you're
    // having to manipulate this then your application is in a state where it
    // has grown beyond EAMain's scope and you would be better served with a
    // custom entry point.
    #ifndef EAMAIN_WINRT_APPLICATION_ENTRY
        #define EAMAIN_WINRT_APPLICATION_ENTRY() { EA::EAMain::Internal::gEAMainFunction = ::EAMain; EA::EAMain::Internal::StartWinRtApplication(); }
    #endif

    namespace EA
    {
        namespace EAMain
        {
            namespace Internal
            {
                EAMAIN_API void StartWinRtApplication();
            }
        }
    }

    [Platform::MTAThread]
    int main(Platform::Array<Platform::String^>^ /*args*/)
    {
        #if defined(EAMAIN_USE_INITFINI)
            EAMainInit();
        #endif

        EA::EAMain::InitializeSignalHandler();

        EAMAIN_WINRT_APPLICATION_ENTRY();

        #if defined(EAMAIN_USE_INITFINI)
            EAMainFini();
        #endif
        return 0;
    }

#elif defined(EA_PLATFORM_WINDOWS)
    #pragma warning(push, 0)
    #include <Windows.h>
    #include <shellapi.h>
    #pragma comment(lib, "shell32.lib")
    #pragma warning(pop)

    int main(int argc, char** argv)
    {
        // To do: Some platforms may require reading the command line from a file.
        // Do we automatically do this for those platforms? Probably try to open a default file.
        // Do we support loading from a file from any platform? Probably so.
    #if defined(EAMAIN_USE_INITFINI)
        EAMainInit();
    #endif

        EA::EAMain::Internal::gEAMainFunction = ::EAMain;

        EA::EAMain::CommandLine commandLine(argc, argv);

        const char *printServerAddress = EA::EAMain::Internal::ExtractPrintServerAddress(commandLine.Argc(), commandLine.Argv());
        EA::EAMain::Internal::EAMainStartup(printServerAddress);

        EA::EAMain::InitializeSignalHandler();

        int returnValue = EA::EAMain::Internal::gEAMainFunction(commandLine.Argc(), commandLine.Argv());

        EA::EAMain::Internal::EAMainShutdown(returnValue);

    #if defined(EAMAIN_USE_INITFINI)
        EAMainFini();
    #endif
        return returnValue;
    }

    int WINAPI wWinMainShared(HINSTANCE /*instance*/, HINSTANCE /*prevInstance*/, LPWSTR wCmdLine, int /*cmdShow*/)
    {
        int       returnValue = 1;
        int       argc  = 0;
        wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), &argc);

        EA::EAMain::Internal::gEAMainFunction = ::EAMain;

        if(wargv)
        {
            char8_t** argv = (char8_t**)_malloca(argc * sizeof(char8_t*));
            __analysis_assume(argv != NULL);

            for(int i = 0; i < argc; i++)
            {
                argv[i] = NULL;
                const int requiredStrlen = EA::StdC::Strlcpy(argv[i], wargv[i], 0);
                argv[i] = (char8_t *)_malloca(sizeof(char8_t) * (requiredStrlen + 1));
                EA::StdC::Strlcpy(argv[i], wargv[i], requiredStrlen + 1);
            }

            #if defined(EAMAIN_USE_INITFINI)
                EAMainInit();
            #endif

            const char *printServerAddress = EA::EAMain::Internal::ExtractPrintServerAddress(argc, argv);
            EA::EAMain::Internal::EAMainStartup(printServerAddress);

            EA::EAMain::InitializeSignalHandler();

            returnValue = EA::EAMain::Internal::gEAMainFunction(argc, argv);
            EA::EAMain::Internal::EAMainShutdown(returnValue);

            #if defined(EAMAIN_USE_INITFINI)
                EAMainFini();
            #endif
            LocalFree(wargv);
        }

        return returnValue;
    }

    #if defined(EAMAIN_MAIN_16)
        int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prevInstance, LPWSTR wCmdLine, int cmdShow)
        {
            return wWinMainShared(instance, prevInstance, wCmdLine, cmdShow);
        }
    #else
        int WINAPI WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prevInstance, _In_ LPSTR cmdLine, _In_ int cmdShow)
        {
            size_t   wCapacity = strlen(cmdLine) * 3; // UTF8 -> UCS2 requires at most 3 times the bytes.
            wchar_t* wCmdLine  = (wchar_t*)_malloca(wCapacity);
            EA::StdC::Strlcpy(wCmdLine, cmdLine, wCapacity);

            return wWinMainShared(instance, prevInstance, wCmdLine, cmdShow);
        }
    #endif
#else
    #if defined(EA_PLATFORM_WINDOWS) && defined(EA_MAIN_16)
        int wmain(int argc, wchar_t** wargv)
        {
            EA::EAMain::Internal::gEAMainFunction = ::EAMain;

            // Allocate and convert-copy wargv to argv.
            char8_t** argv = (char8_t**)_malloca(argc * sizeof(char8_t*));

            for(int i = 0; i < argc; i++)
            {
                argv[i] = NULL;
                const int requiredStrlen = EA::StdC::Strlcpy(argv[i], wargv[i], 0);
                argv[i] = (char8_t)_malloca(sizeof(char8_t) * (requiredStrlen + 1));
                EA::StdC::Strlcpy(argv[i], wargv[i], requiredStrlen + 1);
            }

            #if defined(EAMAIN_USE_INITFINI)
                EAMainInit();
            #endif

            const char *printServerAddress = EA::EAMain::Internal::ExtractPrintServerAddress(argc, argv);
            EA::EAMain::Internal::EAMainStartup(printServerAddress);

            EA::EAMain::InitializeSignalHandler();

            int returnValue = EA::EAMain::Internal::gEAMainFunction(argc, argv);
            EA::EAMain::Internal::EAMainShutdown(returnValue);

            #if defined(EAMAIN_USE_INITFINI)
                EAMainFini();
            #endif

            return returnValue;
        }
    #else
        #if !defined(EAMAIN_FORCE_MAIN_USAGE) // If the user defines EAMAIN_FORCE_MAIN_USAGE before #including this file, then main is used even if we don't normally use it for the given platform.
            #if defined(EA_PLATFORM_IPHONE)
                #define main iosMain
                extern "C" int iosMain(int argc, char** argv);
            #endif
        #endif
    #endif

    int main(int argc, char** argv)
    {
        // To do: Some platforms may require reading the command line from a file.
        // Do we automatically do this for those platforms? Probably try to open a default file.
        // Do we support loading from a file from any platform? Probably so.
    #if defined(EAMAIN_USE_INITFINI)
        EAMainInit();
    #endif

        EA::EAMain::Internal::gEAMainFunction = ::EAMain;

        EA::EAMain::CommandLine commandLine(argc, argv);

        const char *printServerAddress = EA::EAMain::Internal::ExtractPrintServerAddress(commandLine.Argc(), commandLine.Argv());
        EA::EAMain::Internal::EAMainStartup(printServerAddress);

        EA::EAMain::InitializeSignalHandler();

        int returnValue = EA::EAMain::Internal::gEAMainFunction(commandLine.Argc(), commandLine.Argv());

        EA::EAMain::Internal::EAMainShutdown(returnValue);

    #if defined(EAMAIN_USE_INITFINI)
        EAMainFini();
    #endif
        return returnValue;
    }

#endif


#endif // header include guard
