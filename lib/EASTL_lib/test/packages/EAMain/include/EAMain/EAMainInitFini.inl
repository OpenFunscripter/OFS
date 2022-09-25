///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#ifndef EAMAIN_EAENTRYPOINTMAIN_INITFINI_H
#define EAMAIN_EAENTRYPOINTMAIN_INITFINI_H


////////////////////////////////////////////////////////////////////////////////
// This is a file that you can #include before #including <EAMain/EAEntryPointMain.inl>
// If you #include this file or otherwise #define EAMAIN_USE_INITFINI then you are
// expected to provide the extern "C" function implementations below in your
// application. These two functions will be called before EAMain and after EAMain
// respectively, to give the application a chance to do things like set up and
// shutdown a custom memory manager.
//
// Example usage:
//     #include <EAMain/EAMainInitFini.inl>
//     #include <EAMain/EAEntryPointMain.inl>
//
//     extern "C" void EAMainInit()
//     {
//         using namespace EA::Allocator;
//
//         EA::EAMain::PlatformStartup();
//         SetGeneralAllocator(&gEAGeneralAllocator); // This example assumes you are using a heap named as such.
//         gEAGeneralAllocator.SetOption(GeneralAllocatorDebug::kOptionEnablePtrValidation, 0);
//         gEAGeneralAllocator.SetAutoHeapValidation(GeneralAllocator::kHeapValidationLevelBasic, 64);
//     }
//
//     void EAMainFini()
//     {
//         if(EA::EAMain::GetVerbosity() > 0)
//             EA::Allocator::gEAGeneralAllocator.TraceAllocatedMemory();
//         EA::EAMain::PlatformShutdown(gTestResult);
//     }
//
//     int EAMain(int argc, char** argv)
//     {
//         . . .
//     }
////////////////////////////////////////////////////////////////////////////////

#ifdef EAMAIN_EAENTRYPOINTMAIN_INL
    #error EAMainInitFini.inl must be included before EAMainEntryPoint.inl
#endif

/// EAMainInit / EAMainFini
///
/// This functions are declared here, but must be defined in user code.
///
extern "C" void EAMainInit();
extern "C" void EAMainFini();


/// EAMAIN_USE_INITFINI
///
/// Defined or not defined. When defined it directs EAEntryPointMain.inl to call
/// EAMainInit before EAMain and call EAMainFini after EAMain returns.
///
#if !defined(EAMAIN_USE_INITFINI)
    #define EAMAIN_USE_INITFINI 1
#endif


#endif // header include guard
