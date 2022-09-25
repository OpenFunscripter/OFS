///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////


#include <EAMainPrintf.h>
#include <EAMain/internal/EAMainPrintManager.h>
#include <EAStdC/EASprintf.h>

namespace EA
{
namespace EAMain
{
    class IChannel;

namespace Messages
{
    void Print(const char* pData)
    {
        PrintManager::Instance().Send(pData);
    }

}}}  // namespaces
