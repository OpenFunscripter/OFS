///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#include <EABase/eabase.h>
#include <EAMain/EAMain.h>

EA_DISABLE_ALL_VC_WARNINGS()
#include <new>
EA_RESTORE_ALL_VC_WARNINGS()

// Array new is a requirement brought in by EAStdC.
void* operator new[](size_t size, const char* /*pName*/, int /*flags*/, unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
    return ::operator new[](size);
}

///////////////////////////////////////////////////////////////////////////////
// EAMain
//
#if (defined(EA_PLATFORM_MICROSOFT) && !defined(CS_UNDEFINED_STRING) && !EA_WINAPI_FAMILY_PARTITION(EA_WINAPI_PARTITION_DESKTOP))
[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
#elif defined(EA_PLATFORM_IPHONE)
extern "C" int iosMain(int, char **)
#else
int main(int, char**)
#endif
{
    using namespace EA::EAMain;

    Report("Test of EAMain without an entry point has succeeded.\n");

    return 0;
}
