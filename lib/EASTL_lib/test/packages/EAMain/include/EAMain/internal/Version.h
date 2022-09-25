///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#ifndef EAMAIN_INTERNAL_VERSION_H
#define EAMAIN_INTERNAL_VERSION_H


#include <EABase/eabase.h>


///////////////////////////////////////////////////////////////////////////////
// EAMAIN_VERSION
//
// EAMain, at least from version `2.15.0`, adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
//
// Example usage:
//     printf("EAMAIN version: %s", EAMAIN_VERSION);
//     printf("EAMAIN version: %d.%d.%d", EAMAIN_VERSION_N / 10000 % 100, EAMAIN_VERSION_N / 100 % 100, EAMAIN_VERSION_N % 100);
//
///////////////////////////////////////////////////////////////////////////////

#ifndef EAMAIN_VERSION
    #define EAMAIN_VERSION    "3.01.01"
    #define EAMAIN_VERSION_N  ((EA_MAIN_VERSION_MAJOR * 10000) + (EA_MAIN_VERSION_MINOR * 100) + (EA_MAIN_VERSION_PATCH))
#endif

#if defined _MSC_VER
    #pragma once
#endif

/*
This file provides a version number of the EAMain package for your own code to check against.

Major, minor and patch versions are defined and updated each release.
*/

// Define the major, minor and patch versions.
// This information is updated with each release.

//! This define indicates the major version number for the package.
//! \sa EA_MAIN_VERSION
#define EA_MAIN_VERSION_MAJOR   3
//! This define indicates the minor version number for the package.
//! \sa EA_MAIN_VERSION
#define EA_MAIN_VERSION_MINOR   0
//! This define indicates the patch version number for the package.
//! \sa EA_MAIN_VERSION
#define EA_MAIN_VERSION_PATCH   1
//! This define can be used for convenience when printing the version number
//! \sa EA_MAIN_VERSION
#define EA_MAIN_VERSION_STRING EA_MAIN_VERSION

/*!
 * This is a utility macro that users may use to create a single version number
 * that can be compared against EA_MAIN_VERSION.
 *
 * For example:
 *
 * \code
 *
 * #if EA_MAIN_VERSION > EA_MAIN_CREATE_VERSION_NUMBER( 1, 1, 0 )
 * printf("EAMain version is greater than 1.1.0.\n");
 * #endif
 *
 * \endcode
 */
#define EA_MAIN_CREATE_VERSION_NUMBER( major_ver, minor_ver, patch_ver ) \
    ((major_ver) * 1000000 + (minor_ver) * 1000 + (patch_ver))

/*!
 * This macro is an aggregate of the major, minor and patch version numbers.
 * \sa EA_MAIN_CREATE_VERSION_NUMBER
 */
#define EA_MAIN_VERSION \
    EA_MAIN_CREATE_VERSION_NUMBER( EA_MAIN_VERSION_MAJOR, EA_MAIN_VERSION_MINOR, EA_MAIN_VERSION_PATCH )


///////////////////////////////////////////////////////////////////////////////
// EAMAIN_DLL
//
// Defined as 0 or 1. The default is dependent on the definition of EA_DLL.
// If EA_DLL is defined, then EAMAIN_DLL is 1, else EAMAIN_DLL is 0.
// EA_DLL is a define that controls DLL builds within the EAConfig build system.
//
#ifndef EAMAIN_DLL
    #if defined(EA_DLL)
        #define EAMAIN_DLL 1
    #else
        #define EAMAIN_DLL 0
    #endif
#endif

#ifndef EAMAIN_API // If the build file hasn't already defined this to be dllexport...
    #if defined(EAMAIN_DLL) && EAMAIN_DLL
        #if defined(_MSC_VER)
            #define EAMAIN_API      __declspec(dllimport)
            #define EAMAIN_LOCAL
        #elif defined(__CYGWIN__)
            #define EAMAIN_API      __attribute__((dllimport))
            #define EAMAIN_LOCAL
        #elif (defined(__GNUC__) && (__GNUC__ >= 4))
            #define EAMAIN_API      __attribute__ ((visibility("default")))
            #define EAMAIN_LOCAL    __attribute__ ((visibility("hidden")))
        #else
            #define EAMAIN_API
            #define EAMAIN_LOCAL
        #endif

    #else
        #define EAMAIN_API
        #define EAMAIN_LOCAL
    #endif
#endif

#endif // Header include guard
