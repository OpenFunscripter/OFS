///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

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
#endif

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

void* operator new[](size_t size, const char* /*pName*/, int /*flags*/, unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
    return operator new[](size);
}

void* operator new[](size_t size, size_t /*alignment*/, size_t /*alignmentOffset*/, const char* /*pName*/,
                        int /*flags*/, unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
    return operator new[](size);
}

void CrashHelper(int *ptr);

int EAMain(int argc, char** argv)
{
    CrashHelper(NULL);
    return 0;
}
