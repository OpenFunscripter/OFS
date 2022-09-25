///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

void MyCustomWinRtEntry();

#define EAMAIN_WINRT_APPLICATION_ENTRY MyCustomWinRtEntry
#include <EAAssert/eaassert.h>
#include <EABase/eabase.h>
#include <EAMain/EAMain.h>
#include <EAMain/EAEntryPointMain.inl>
#include <string.h>

#ifdef _MSC_VER
    #pragma warning(push, 0)
#endif

#include <stdio.h>
#include <stdlib.h>
#if defined(EA_COMPILER_MSVC) && defined(EA_PLATFORM_MICROSOFT)
    #include <crtdbg.h>
    #include <Windows.h>
#endif

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

#include "eathread/eathread.h"

void* operator new[](size_t size, const char* /*pName*/, int /*flags*/, unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
    return operator new[](size);
}

void* operator new[](size_t size, size_t /*alignment*/, size_t /*alignmentOffset*/, const char* /*pName*/, 
                        int /*flags*/, unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
    return operator new[](size);
}

///////////////////////////////////////////////////////////////////////////////
// EAMain
//

ref class TestApplicationView sealed : public Windows::ApplicationModel::Core::IFrameworkView
{
public:
    TestApplicationView()
        : mCommandLine("")
    {}

    // IFrameworkView Methods
    virtual void Initialize(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView) 
    {
        using namespace Windows::ApplicationModel::Activation;
        using namespace Windows::ApplicationModel::Core;
        applicationView->Activated += ref new Windows::Foundation::TypedEventHandler< CoreApplicationView^, IActivatedEventArgs^ >( this, &TestApplicationView::OnActivated );
    }
    virtual void SetWindow(Windows::UI::Core::CoreWindow^ window) {}
    virtual void Load(Platform::String^ entryPoint) {}
    virtual void Run();
    virtual void Uninitialize() {}
    void OnActivated( Windows::ApplicationModel::Core::CoreApplicationView^ applicationView, Windows::ApplicationModel::Activation::IActivatedEventArgs^ args ) {
        if (args->Kind == Windows::ApplicationModel::Activation::ActivationKind::Launch) 
        { 
            Windows::ApplicationModel::Activation::LaunchActivatedEventArgs ^launchArgs = (Windows::ApplicationModel::Activation::LaunchActivatedEventArgs ^) args;
            Platform::String ^argumentString = launchArgs->Arguments;

            int bufferSize = WideCharToMultiByte(
                    CP_UTF8,
                    0,
                    argumentString->Data(), 
                    argumentString->Length(),
                    NULL,
                    0,
                    NULL,
                    NULL);

            mCommandLine = new char[bufferSize + 1];
            int rv = WideCharToMultiByte(
                    CP_UTF8,
                    0,
                    argumentString->Data(),
                    argumentString->Length(),
                    mCommandLine,
                    bufferSize + 1,
                    NULL,
                    NULL);

            mCommandLine[bufferSize] = 0;
            EA_ASSERT(rv == bufferSize);
            EA_UNUSED(rv);
        } 
        Windows::UI::Core::CoreWindow::GetForCurrentThread()->Activate();
    }

private:
    char *mCommandLine;
};

void TestApplicationView::Run()
{
    using namespace EA::EAMain;

    IWinRTRunner *runner = CreateWinRTRunner();
    CommandLine commandLine(mCommandLine, CommandLine::FLAG_NO_PROGRAM_NAME);

    runner->Run(commandLine.Argc(), commandLine.Argv());

    while (!runner->IsFinished())
    {
        EA::Thread::ThreadSleep(1);
    }

    Windows::ApplicationModel::Core::CoreApplication::Exit();
}

ref class TestApplicationViewSource : Windows::ApplicationModel::Core::IFrameworkViewSource
{
public:
    virtual Windows::ApplicationModel::Core::IFrameworkView^ CreateView() 
    {
        return ref new TestApplicationView(); 
    }
};

int CustomWinRtEntryEAMain(int argc, char **argv)
{
    EA::EAMain::Report("Success!\n");

    return 0;
}

void MyCustomWinRtEntry()
{
    EA::EAMain::Internal::gEAMainFunction = CustomWinRtEntryEAMain;
    Windows::ApplicationModel::Core::CoreApplication::Run(ref new TestApplicationViewSource);
}

