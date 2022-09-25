///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#ifndef IPHONEENTRY_H
#define IPHONEENTRY_H

#pragma once

namespace EA
{
    namespace EAMain
    {
        // Allows the user to specify their own custom Application Delegate, to
        // replace the one defined in IPhoneEntry.mm. Be warned that if you want
        // to run tests on the EATech Build Farm, you may need to implement some
        // of the behaviours of the EAMainAppDelegate in your custom delegate,
        // specifically the "Tests complete" alert.
        void SetAppDelegateName(const char* delegateName);

        // Accessors for command-line arguments
        int GetArgC();
        char** GetArgV();
    }
}
#endif // Header include guard
