///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#include "EAMain/EAMain.h"
#include "EABase/eabase.h"
#include "EAAssert/eaassert.h"
#include <EAMain/internal/EAMainPrintManager.h>
#include <EAMain/internal/EAMainChannels.h>

#include <EABase/eabase.h>

EA_DISABLE_ALL_VC_WARNINGS()

#include <stdio.h>
#include <string.h>

EA_RESTORE_ALL_VC_WARNINGS()

namespace EA {
namespace EAMain {

//------------------------------------------------------------
// Printf Channel
//------------------------------------------------------------
void PrintfChannel::Send(const char8_t* pData)
{
    // Route to default print function
    EA::EAMain::GetDefaultReportFunction()(pData);
}

//------------------------------------------------------------
// File Channel
//------------------------------------------------------------
void FileChannel::Init()
{
    mFileHandle = fopen("eamain_output.txt", "w");
    EA_ASSERT_MSG(mFileHandle, "invalid file handle");
}

//------------------------------------------------------------
void FileChannel::Send(const char8_t* pData)
{
    EA_ASSERT_MSG(mFileHandle, "invalid file handle");
    fputs(pData, mFileHandle);
}

//------------------------------------------------------------
void FileChannel::Shutdown()
{
    EA_ASSERT_MSG(mFileHandle, "invalid file handle");
    fclose(mFileHandle);
}

//------------------------------------------------------------

}}
