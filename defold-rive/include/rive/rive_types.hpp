/*
 * Copyright 2022 Rive
 */

// This should always be included by any other rive files,
// as it performs basic self-consistency checks, and provides
// shared common types and macros.

#ifndef _RIVE_TYPES_HPP_
#define _RIVE_TYPES_HPP_

// clang-format off

#if defined(DEBUG) && defined(NDEBUG)
    #error "can't determine if we're debug or release"
#endif

#if !defined(DEBUG) && !defined(NDEBUG)
    // we have to make a decision what mode we're in
    // historically this has been to look for NDEBUG, and in its
    // absence assume we're DEBUG.
    #define DEBUG  1
    // fyi - Xcode seems to set DEBUG (or not), so the above guess
    // doesn't work for them - so our projects may need to explicitly
    // set NDEBUG in our 'release' builds.
#endif

#ifdef NDEBUG
    #ifndef RELEASE
        #define RELEASE 1
    #endif
#else   // debug mode
    #ifndef DEBUG
        #define DEBUG   1
    #endif
#endif

// Some checks to guess what platform we're building for

#ifdef __APPLE__

    #define RIVE_BUILD_FOR_APPLE
    #include <TargetConditionals.h>

    #if TARGET_OS_IPHONE
        #define RIVE_BUILD_FOR_IOS
    #elif TARGET_OS_MAC
        #define RIVE_BUILD_FOR_OSX
    #endif

#endif

// We really like these headers, so we include them all the time.

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

// Annotations to assert unreachable control flow.
#ifdef __GNUC__ // GCC 4.8+, Clang, Intel and others compatible with GCC (-std=c++0x or above)
    #define RIVE_UNREACHABLE __builtin_unreachable()
#elif _MSC_VER
    #define RIVE_UNREACHABLE __assume(0)
#else
    #define RIVE_UNREACHABLE do {} while(0)
#endif

// clang-format on

#endif // rive_types
