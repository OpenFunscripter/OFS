///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////


#include <EABase/eabase.h>
#include <EAMain/EAMain.h>
#include <EAMain/EAMainInitFini.inl>
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

static int TestCommandLineArgs()
{
    using namespace EA::EAMain;

    #define EAMAIN_TEST(x) if (!(x)) { Report("%s(%d): %s\n", __FILE__, __LINE__, #x); ++nErrorCount; } else (void)0
    #define EAMAIN_TEST_FATAL(x) if (!(x)) { Report("%s(%d): %s\n", __FILE__, __LINE__, #x); ++nErrorCount; return nErrorCount; } else (void)0

    int nErrorCount = 0;

    // This test is disabled on xenon because xenon has implicit command line
    // construction if argc is zero.
    {
        int argc = 0;
        char *argv[] = { NULL };

        CommandLine commandLine(argc, argv);

        EAMAIN_TEST(commandLine.Argc() == 0);
        EAMAIN_TEST(commandLine.Argv() != NULL);
        EAMAIN_TEST(commandLine.FindSwitch("-x") == -1);
        EAMAIN_TEST(commandLine.Arg(0) == nullptr);
        EAMAIN_TEST(commandLine.Argv()[0] == nullptr);
    }

    {
        int argc = 1;
        char arg[] = "program.elf";
        char *argv[] = { arg };

        CommandLine commandLine(argc, argv);

        EAMAIN_TEST(commandLine.Argc() == 1);
        EAMAIN_TEST_FATAL(commandLine.Argv() != NULL);
        EAMAIN_TEST(strcmp(commandLine.Argv()[0], "program.elf") == 0);
        EAMAIN_TEST(strcmp(commandLine.Arg(0), "program.elf") == 0);
        EAMAIN_TEST(commandLine.FindSwitch("-x") == -1);
        EAMAIN_TEST(commandLine.Arg(1) == nullptr);
        EAMAIN_TEST(commandLine.Argv()[1] == nullptr);
    }

    {
        const char *commandLineString = "program.elf";

        CommandLine commandLine(commandLineString);

        EAMAIN_TEST(commandLine.Argc() == 1);
        EAMAIN_TEST_FATAL(commandLine.Argv() != NULL);
        EAMAIN_TEST(strcmp(commandLine.Argv()[0], "program.elf") == 0);
        EAMAIN_TEST(strcmp(commandLine.Arg(0), "program.elf") == 0);
        EAMAIN_TEST(commandLine.FindSwitch("-x") == -1);
        EAMAIN_TEST(commandLine.Arg(1) == nullptr);
        EAMAIN_TEST(commandLine.Argv()[1] == nullptr);
    }

    {
        const char *commandLineString = "program.elf arg1 \"arg 2\"";

        CommandLine commandLine(commandLineString);

        EAMAIN_TEST(commandLine.Argc() == 3);
        EAMAIN_TEST_FATAL(commandLine.Argv() != NULL);
        EAMAIN_TEST(strcmp(commandLine.Argv()[0], "program.elf") == 0);
        EAMAIN_TEST(strcmp(commandLine.Arg(0), "program.elf") == 0);
        EAMAIN_TEST(strcmp(commandLine.Arg(1), "arg1") == 0);
        EAMAIN_TEST(strcmp(commandLine.Arg(2), "arg 2") == 0);
        EAMAIN_TEST(commandLine.FindSwitch("-x") == -1);
        EAMAIN_TEST(commandLine.Arg(3) == nullptr);
        EAMAIN_TEST(commandLine.Argv()[3] == nullptr);
    }

    {
        const char *commandLineString = "program.elf -x -y:1";
        const char *parameter = NULL;

        CommandLine commandLine(commandLineString);

        EAMAIN_TEST(commandLine.Argc() == 3);
        EAMAIN_TEST_FATAL(commandLine.Argv() != NULL);
        EAMAIN_TEST(strcmp(commandLine.Arg(0), "program.elf") == 0);
        EAMAIN_TEST(strcmp(commandLine.Arg(1), "-x") == 0);
        EAMAIN_TEST(commandLine.FindSwitch("-x") == 1);
        EAMAIN_TEST(commandLine.FindSwitch("-y", false, &parameter) == 2);
        EAMAIN_TEST_FATAL(parameter != NULL);
        EAMAIN_TEST(strcmp(parameter, "1") == 0);
        EAMAIN_TEST(commandLine.Arg(3) == nullptr);
        EAMAIN_TEST(commandLine.Argv()[3] == nullptr);
    }

    {
        const char *commandLineString = "program.elf -x \"-switch:this switch parameter has spaces\"";
        const char *parameter = NULL;

        CommandLine commandLine(commandLineString);

        EAMAIN_TEST(commandLine.Argc() == 3);
        EAMAIN_TEST_FATAL(commandLine.Argv() != NULL);
        EAMAIN_TEST(strcmp(commandLine.Arg(0), "program.elf") == 0);
        EAMAIN_TEST(strcmp(commandLine.Arg(1), "-x") == 0);
        EAMAIN_TEST(commandLine.FindSwitch("-x") == 1);
        EAMAIN_TEST(commandLine.FindSwitch("-switch", false, &parameter) == 2);
        EAMAIN_TEST_FATAL(parameter != NULL);
        EAMAIN_TEST(strcmp(parameter, "this switch parameter has spaces") == 0);
        EAMAIN_TEST(commandLine.Arg(3) == nullptr);
        EAMAIN_TEST(commandLine.Argv()[3] == nullptr);
    }

    {
        const char *commandLineString = "program.elf -x -switch:\"this switch parameter has spaces\" \"-switch2:as does this one\"";
        const char *parameter = NULL;

        CommandLine commandLine(commandLineString);

        EAMAIN_TEST(commandLine.Argc() == 4);
        EAMAIN_TEST_FATAL(commandLine.Argv() != NULL);
        EAMAIN_TEST(strcmp(commandLine.Arg(0), "program.elf") == 0);
        EAMAIN_TEST(strcmp(commandLine.Arg(1), "-x") == 0);
        EAMAIN_TEST(commandLine.FindSwitch("-x") == 1);
        EAMAIN_TEST(commandLine.FindSwitch("-switch", false, &parameter) == 2);
        EAMAIN_TEST_FATAL(parameter != NULL);
        EAMAIN_TEST(strcmp(parameter, "this switch parameter has spaces") == 0);
        EAMAIN_TEST(commandLine.Arg(4) == nullptr);
        EAMAIN_TEST(commandLine.Argv()[4] == nullptr);
    }

    {
        const char *commandLineString = "-x -switch:\"this switch parameter has spaces\" \"-switch2:as does this one\"";
        const char *parameter = NULL;

        CommandLine commandLine(commandLineString, CommandLine::FLAG_NO_PROGRAM_NAME);

        EAMAIN_TEST(commandLine.Argc() == 4);
        EAMAIN_TEST_FATAL(commandLine.Argv() != NULL);
        EAMAIN_TEST(strcmp(commandLine.Arg(0), "") == 0);
        EAMAIN_TEST(strcmp(commandLine.Arg(1), "-x") == 0);
        EAMAIN_TEST(commandLine.FindSwitch("-x") == 1);
        EAMAIN_TEST(commandLine.FindSwitch("-switch", false, &parameter) == 2);
        EAMAIN_TEST_FATAL(parameter != NULL);
        EAMAIN_TEST(strcmp(parameter, "this switch parameter has spaces") == 0);
        EAMAIN_TEST(commandLine.Arg(4) == nullptr);
        EAMAIN_TEST(commandLine.Argv()[4] == nullptr);
    }

    {
        int argc = 3;
        char arg0[] = "program.elf";
        char arg1[] = "-x";
        char arg2[] = "-switch:this switch parameter has spaces";
        char *argv[] = { arg0, arg1, arg2 };
        const char *parameter;

        CommandLine commandLine(argc, argv);

        EAMAIN_TEST(commandLine.Argc() == 3);
        EAMAIN_TEST_FATAL(commandLine.Argv() != NULL);
        EAMAIN_TEST(strcmp(commandLine.Arg(0), "program.elf") == 0);
        EAMAIN_TEST(strcmp(commandLine.Arg(1), "-x") == 0);
        EAMAIN_TEST(commandLine.FindSwitch("-x") == 1);
        EAMAIN_TEST(commandLine.FindSwitch("-switch", false, &parameter) == 2);
        EAMAIN_TEST_FATAL(parameter != NULL);
        EAMAIN_TEST(strcmp(parameter, "this switch parameter has spaces") == 0);
        EAMAIN_TEST(commandLine.Arg(3) == nullptr);
        EAMAIN_TEST(commandLine.Argv()[3] == nullptr);
    }

    return nErrorCount;

    #undef EAMAIN_TEST
    #undef EAMAIN_TEST_FATAL
}

static bool gEAMainInitCalled;

void EAMainInit()
{
    gEAMainInitCalled = true;
}

void EAMainFini()
{
}

///////////////////////////////////////////////////////////////////////////////
// EAMain
//
int EAMain(int argc, char** argv)
{
    using namespace EA::EAMain;

    int nErrorCount(0);

    Report("List of arguments passed:\n");
    for (int i = 0; i < argc; ++i)
    {
        Report("Arg %d: %s\n", i, argv[i]);
    }
    Report("\n");

    // Basic test of redirection of stdout/stderr on platforms that support
    // redirection of these streams.
    printf("printf(...)\n");
    fprintf(stdout, "fprintf(stdout, ...)\n");
    fflush(stdout);

    fprintf(stderr, "fprintf(stderr, ...)\n");
    fflush(stderr);

    Report("Test of %s.\n", "Report()");
    Report("Report()\n");

    bool bArgPassed = false;

    for (int i = 0; i < argc; ++i)
    {
        if (strcmp(argv[i], "-testargpassing") == 0)
        {
            bArgPassed = true;
        }
    }

    // Windows Phone does not support passing arguments to app bundles when
    // launching them.
#if !defined(EA_PLATFORM_WINDOWS_PHONE)
    if (!bArgPassed)
    {
        Report("Arg not passed!\n");
        ++nErrorCount;
    }
#endif

    if (TestCommandLineArgs() != 0)
    {
        Report("Error parsing command line arguments!\n");
        ++nErrorCount;
    }

    if (!gEAMainInitCalled)
    {
        Report("EAMainInit was not called!\n");
        ++nErrorCount;
    }

    Report("This report statement has \nno terminating newline");

    return nErrorCount;
}
