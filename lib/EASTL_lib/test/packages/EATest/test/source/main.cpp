///////////////////////////////////////////////////////////////////////////////
// main.cpp
//
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////


#include <EABase/eabase.h>
#include <EATest/EATest.h>
#include <EAMain/EAEntryPointMain.inl>
#include <EAStdC/EAString.h>

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
    return operator new(size);
}

void* operator new[](size_t size, size_t /*alignment*/, size_t /*alignmentOffset*/, const char* /*pName*/, 
                        int /*flags*/, unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
    return operator new(size);
}

void operator delete[] (void* ptr) EA_THROW_SPEC_DELETE_NONE()
{
    operator delete(ptr);
}


// Test1
// Does nothing except return a value.
class Test1 : public EA::UnitTest::Test
{
public:
    Test1(const char8_t* pTestName, bool bShouldSucceed = true) 
      : Test(pTestName), mnRunCount(10), mbShouldSucceed(bShouldSucceed) {}

    int Run()
    {
        using namespace EA::UnitTest;

        EA_DISABLE_VC_WARNING(6326)
        Verify(1 < 2, "Failure of (1 < 2) comparison.");
        EA_RESTORE_VC_WARNING()

        if(--mnRunCount > 0)
            return kTestResultContinue;

        WriteReport();
        return mbShouldSucceed ? kTestResultOK : kTestResultError;
    }

    int  mnRunCount;
    bool mbShouldSucceed;
};


// Test2
// Does nothing except return a value.
class Test2 : public EA::UnitTest::Test
{
public:
    Test2(const char8_t* pTestName, bool bShouldSucceed = true) 
        : Test(pTestName), mnRunCount(10), mbShouldSucceed(bShouldSucceed) {}

    int Run()
    {
        using namespace EA::UnitTest;

        EA_DISABLE_VC_WARNING(6326)
        Verify(1 < 2, "Failure of (1 < 2) comparison.");
        EA_RESTORE_VC_WARNING()

        if(--mnRunCount > 0)
            return kTestResultContinue;

        WriteReport();
        return mbShouldSucceed ? kTestResultOK : kTestResultError;
    }

    int  mnRunCount;
    bool mbShouldSucceed;
};



// TestFunction1
// Does nothing except return a value.
static int TestFunction1()
{
    using namespace EA::UnitTest;

    static int nRunCount(10);

    if(--nRunCount > 0)
        return kTestResultContinue;

    nRunCount = 10;
    return kTestResultOK;
}


// TestFunction1
// Does nothing except return a value.
static int TestFunction2()
{
    return 0;
}

static int TestFunction3()
{
    return 0;
}

static int TestFunction4()
{
    return 0;
}



// TestClass1
// Does nothing except return a value.
struct TestClass1
{
    int DoTest();
};

int TestClass1::DoTest()
{
    return 0;
}


///////////////////////////////////////////////////////////////////////////////
// EAMain
//
static int TestMisc()
{
    int nErrorCount(0);

    using namespace EA::UnitTest;

    { // Test standalone functions.

        // In many of these cases we don't validate the results but rather simply make sure they return rational values and don't crash.
        bool result = EA::UnitTest::IsDebuggerPresent();
        Report("Debugger is %s.\n", result ? "present" : "absent");

        result = IsUserAdmin();
        Report("User is currently %s.\n", result ? "admin" : "not admin");

        unsigned verbosity = GetVerbosity();
        Report("Test verbosity is %d.\n", verbosity);

        ThreadSleep(100);
        ThreadSleepRandom(10, 20, false);
        ThreadSleepRandom(10, 20, true);

        uint64_t time64 = GetSystemTimeMicroseconds();
        Report("System time us = %I64u.\n", time64);

        if (!IsRunningUnderValgrind())
        {
            SetHighThreadPriority();
            SetNormalThreadPriority();
          //SetLowProcessPriority(); // Currently disabled because we don't currently have a way to set it back to normal priority.
        }

        if(GetInteractive())
            MessageBoxAlert("Message box test", "Message Box Test");

        NonInlinableFunction();

        int& value = WriteToEnsureFunctionCalled();
        EA_UNUSED(value);

        float systemSpeed = GetSystemSpeed(kSpeedTypeCPU);
        Report("System kSpeedTypeCPU %.1f.\n", systemSpeed);
        systemSpeed = GetSystemSpeed(kSpeedTypeFPU);
        Report("System kSpeedTypeFPU %.1f.\n", systemSpeed);
        systemSpeed = GetSystemSpeed(kSpeedTypeGPU);
        Report("System kSpeedTypeGPU %.1f.\n", systemSpeed);
        systemSpeed = GetSystemSpeed(kSpeedTypeDisk);
        Report("System kSpeedTypeDisk %.1f.\n", systemSpeed);

        uint64_t memory64 = GetSystemMemoryMB();
        Report("System memory %I64u.\n", memory64);

        uint32_t seed = GetRandSeed();
        Report("Test random seed %I32u.\n", seed);
    }

    { // Test the Report functions.
        Report(NULL, "Test of %s.\n", "Report()");
        Report("Test of %s.\n", "Report()");
        Report("Report()\n");
    }


    { // Test the VERIFY macro.
        EATEST_VERIFY(__LINE__ != 0);
        EATEST_VERIFY(1 != 2);
        EATEST_VERIFY(((uintptr_t)&nErrorCount % 2) == 0);

        EATEST_VERIFY_MSG(__LINE__ != 0, "EATEST_VERIFY_MSG");
        EATEST_VERIFY_MSG(1 != 2, "EATEST_VERIFY_MSG");
        EATEST_VERIFY_MSG(((uintptr_t)&nErrorCount % 2) == 0, "EATEST_VERIFY_MSG");

        #if !defined(EA_COMPILER_NO_VARIADIC_MACROS)
            EATEST_VERIFY_F(__LINE__ != 0, "%s", "EATEST_VERIFY_F");
            EATEST_VERIFY_F(1 != 2, "%s", "EATEST_VERIFY_F");
            EATEST_VERIFY_F(((uintptr_t)&nErrorCount % 2) == 0, "%s", "EATEST_VERIFY_F");
        #endif
    }


    { // Test class Test
        // Test for OK return.
        Test1 test1OK("Test1", true);
        int nResult;

        while((nResult = test1OK.Run()) == kTestResultContinue)
            ; // Do nothing.
        EATEST_VERIFY_MSG(nResult == kTestResultOK, "Failure in test1OK.");


        // Test for Error return.
        Test1 test1Error("Test1", false);

        while((nResult = test1Error.Run()) == kTestResultContinue)
            ; // Do nothing.
        EATEST_VERIFY_MSG(nResult == kTestResultError, "Failure in test1Error.");
    }


    { // Test class TestFunction
        TestFunction testFunction("TestFunction1", TestFunction1);
        int nResult;

        while((nResult = testFunction.Run()) == kTestResultContinue)
            ; // Do nothing.
        EATEST_VERIFY_MSG(nResult == kTestResultOK, "Failure in TestFunction.");
    }


    { // Test class TestMemberFunction
        TestClass1 testClass1;
        TestMemberFunction<TestClass1> class1Test("Test of TestClass1", &testClass1, &TestClass1::DoTest);

        const int nResult = class1Test.Run();
        EATEST_VERIFY_MSG(nResult == kTestResultOK, "Failure in TestClass1.");
    }


    { // Test class TestSuite
        TestSuite testSuite("Test suite");
        int nResult;

        // Test objects
        Test1 test1("Test1");
        testSuite.AddTest(&test1, false);
        testSuite.AddTest(new Test2("Test2"), true);

        // Test functions
        testSuite.AddTest("TestFunction1", TestFunction1);

        // Test member functions
        TestClass1 testClass1;
        testSuite.AddTest("Test of TestClass1", &testClass1, &TestClass1::DoTest);

        // Test enumeration
        size_t n1 = testSuite.EnumerateTests(0, 100);
        EATEST_VERIFY_MSG(n1 == 4, "Failure in TestSuite.");

        n1 = testSuite.EnumerateTests(NULL, 0);  // Verify that NULL is treated as expected.
        EATEST_VERIFY_MSG(n1 == 4, "Failure in TestSuite.");

        n1 = testSuite.EnumerateTests(NULL, 100); // Verify that NULL is treated as expected.
        EATEST_VERIFY_MSG(n1 == 4, "Failure in TestSuite.");

        // Run tests
        while((nResult = testSuite.Run()) == kTestResultContinue)
            ; // Do nothing.
        EATEST_VERIFY_MSG(nResult == kTestResultOK, "Failure in TestSuite.");
    }

    { // Test Rand
        Rand rng(100);

        for(int i = 0; i < 1000; i++)
        {
            /*int32_t y =*/ rng.RandValue();
            // Hard to verify without full-blown randomness testing.

            uint32_t z = rng.RandLimit(1000);
            EATEST_VERIFY(z < 1000);

            int32_t w = rng.RandRange(-50, +30);
            EATEST_VERIFY((w >= -50) && (w < 30));

            /*uint32_t x =*/ rng();
            // Hard to verify without full-blown randomness testing.

            uint32_t q = rng(100);
            EATEST_VERIFY(q < 100);
        }

        uint32_t* pFirst = NULL, *pLast = NULL;
        eastl::random_shuffle(pFirst, pLast, rng);
    }

    return nErrorCount;
}


///////////////////////////////////////////////////////////////////////////////
// EAMain
//
int EAMain(int argc, char** argv)
{
    using namespace EA::UnitTest;

    int  nErrorCount(0);
    { // Test of the TestApplication class and the CommandLine class.
        TestApplication testSuite("Test Unit Tests", argc, argv);

        // Add all tests
        testSuite.AddTest("Misc",      TestMisc);       
        testSuite.AddTest("Function1", TestFunction1);
        testSuite.AddTest("Function2", TestFunction2);
        testSuite.AddTest("Function3", TestFunction3);
        testSuite.AddTest("Function4", TestFunction4);       
				
        // Startup info
		Report("**************************************************************************\n");
        Report("* EATest test                                                            *\n");
        Report("*                                                                        *\n");
        Report("* Available arguments:                                                   *\n");
        Report("*   -list            Displays a list of available tests.                 *\n");
        Report("*   -run:<TestName>  Runs a specified test or all if no name is present. *\n"); 
        Report("*   -wait[:yes | no] Waits at the end for a user confirmation.           *\n");
        Report("*                                                                        *\n");
        Report("* Example usage:                                                         *\n");
        Report("*   EATestTest.exe -list -wait                                           *\n");
        Report("*   EATestTest.exe -run:DateTime -run:Random                             *\n");
        Report("*   EATestTest.exe -run:DateTime -wait:no                                *\n");
        Report("*                                                                        *\n");
        Report("* Available tests:                                                       *\n");
        testSuite.PrintTestNames(true);
        Report("**************************************************************************\n\n");

        const int testResult = testSuite.Run();
        EATEST_VERIFY(testResult != 0x123456);
    }

    return nErrorCount;
}












