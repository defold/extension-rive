// Copyright 2020-2024 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_RIVE_CRASH_H
#define DM_RIVE_CRASH_H

#include "jni/jni.h"

#if defined(__APPLE__) || defined(__linux__)
#include <signal.h>
#endif

namespace dmRiveCrash
{
    void HandleCppException(JNIEnv* env, const char* context, const char* message);
    void HandleUnknownCppException(JNIEnv* env, const char* context);

#if defined(__APPLE__) || defined(__linux__)
    struct SignalHandlerState
    {
        bool installed;
        struct sigaction old_sigsegv;
        struct sigaction old_sigabrt;
        struct sigaction old_sigbus;
        struct sigaction old_sigill;
    };

    class ScopedSignalHandler
    {
    public:
        ScopedSignalHandler();
        ~ScopedSignalHandler();
    private:
        SignalHandlerState m_State;
    };
#else
    class ScopedSignalHandler
    {
    public:
        ScopedSignalHandler() {}
        ~ScopedSignalHandler() {}
    };
#endif
}

#endif
