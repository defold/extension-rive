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

#include "crash.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(__APPLE__) || defined(__linux__)
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#endif

namespace dmRiveCrash
{

static void PrintCallstack()
{
#if defined(__APPLE__) || defined(__linux__)
    void* frames[64];
    int frame_count = backtrace(frames, (int)(sizeof(frames) / sizeof(frames[0])));
    char** symbols = backtrace_symbols(frames, frame_count);
    if (symbols)
    {
        printf("Callstack (%d frames):\n", frame_count);
        for (int i = 0; i < frame_count; ++i)
        {
            printf("  %s\n", symbols[i]);
        }
        free(symbols);
    }
    else
    {
        printf("Callstack unavailable (backtrace_symbols failed)\n");
    }
#else
    printf("Callstack unavailable on this platform\n");
#endif
}

#if defined(__APPLE__) || defined(__linux__)
static void PrintCallstackFromSignal()
{
    void* frames[64];
    int frame_count = backtrace(frames, (int)(sizeof(frames) / sizeof(frames[0])));
    static const char header[] = "Callstack (signal):\n";
    write(STDOUT_FILENO, header, sizeof(header) - 1);
    const int skip = 3; // Skip handler frames.
    if (frame_count > skip)
    {
        backtrace_symbols_fd(frames + skip, frame_count - skip, STDOUT_FILENO);
    }
}

static void PrintCallstackUnsafe()
{
    void* frames[64];
    int frame_count = backtrace(frames, (int)(sizeof(frames) / sizeof(frames[0])));
    fprintf(stdout, "Callstack (signal, demangled):\n");
    const int skip = 3; // Skip handler frames.
    for (int i = skip; i < frame_count; ++i)
    {
        Dl_info info;
        memset(&info, 0, sizeof(info));
        const void* addr = frames[i];
        const char* image = "<unknown>";
        const char* symbol = "<unknown>";
        uintptr_t offset = 0;

        if (dladdr(addr, &info) && info.dli_sname)
        {
            image = info.dli_fname ? info.dli_fname : image;
            symbol = info.dli_sname;
            if (info.dli_saddr)
            {
                offset = (uintptr_t)addr - (uintptr_t)info.dli_saddr;
            }
        }

        int status = 0;
        char* demangled = abi::__cxa_demangle(symbol, 0, 0, &status);
        const char* final_symbol = (status == 0 && demangled) ? demangled : symbol;

        fprintf(stdout, "  #%02d %p %s + 0x%lx (%s)\n", i, addr, final_symbol, (unsigned long)offset, image);
        if (demangled)
            free(demangled);
    }
    fflush(stdout);
}

static int g_SignalDumpFd = -1;
static SignalHandlerState g_SignalState;
static int g_SignalStateDepth = 0;

static bool ShouldDumpSignalFile()
{
    return g_SignalDumpFd != -1;
}

static void WriteAll(int fd, const char* data, size_t len)
{
    while (len > 0)
    {
        ssize_t written = write(fd, data, len);
        if (written <= 0)
            return;
        data += written;
        len -= (size_t)written;
    }
}

static void DumpFrameToFile(int index, void* addr)
{
    if (!ShouldDumpSignalFile())
        return;

    Dl_info info;
    memset(&info, 0, sizeof(info));
    const char* image = "<unknown>";
    uintptr_t base = 0;
    const char* symbol = "<unknown>";
    if (dladdr(addr, &info) && info.dli_fname)
    {
        image = info.dli_fname;
        base = (uintptr_t)info.dli_fbase;
        if (info.dli_sname)
        {
            symbol = info.dli_sname;
        }
    }

    int status = 0;
    char* demangled = abi::__cxa_demangle(symbol, 0, 0, &status);
    const char* final_symbol = (status == 0 && demangled) ? demangled : symbol;

    char line[1024];
    int len = snprintf(line, sizeof(line), "frame|%d|0x%lx|0x%lx|%s|%s\n",
                       index,
                       (unsigned long)(uintptr_t)addr,
                       (unsigned long)base,
                       image,
                       final_symbol);
    if (len > 0)
    {
        WriteAll(g_SignalDumpFd, line, (size_t)len);
    }
    if (demangled)
        free(demangled);
}

static void DumpCallstackToFile()
{
    if (!ShouldDumpSignalFile())
        return;

    void* frames[64];
    int frame_count = backtrace(frames, (int)(sizeof(frames) / sizeof(frames[0])));
    const int skip = 3; // Skip handler frames.
    int out_index = 0;
    for (int i = skip; i < frame_count; ++i, ++out_index)
    {
        DumpFrameToFile(out_index, frames[i]);
    }
}

static void WriteLiteral(const char* msg, size_t len)
{
    write(STDOUT_FILENO, msg, len);
}

static void SignalHandler(int signum, siginfo_t* info, void* context)
{
    static const char msg_sigsegv[] = "Caught SIGSEGV\n";
    static const char msg_sigabrt[] = "Caught SIGABRT\n";
    static const char msg_sigbus[] = "Caught SIGBUS\n";
    static const char msg_sigill[] = "Caught SIGILL\n";
    static const char msg_sigfatal[] = "Caught fatal signal\n";

    if (signum == SIGSEGV)
        WriteLiteral(msg_sigsegv, sizeof(msg_sigsegv) - 1);
    else if (signum == SIGABRT)
        WriteLiteral(msg_sigabrt, sizeof(msg_sigabrt) - 1);
    else if (signum == SIGBUS)
        WriteLiteral(msg_sigbus, sizeof(msg_sigbus) - 1);
    else if (signum == SIGILL)
        WriteLiteral(msg_sigill, sizeof(msg_sigill) - 1);
    else
        WriteLiteral(msg_sigfatal, sizeof(msg_sigfatal) - 1);

    PrintCallstackUnsafe();
    if (ShouldDumpSignalFile())
    {
        DumpCallstackToFile();
    }

    const struct sigaction* old_action = 0;
    switch (signum)
    {
        case SIGSEGV: old_action = &g_SignalState.old_sigsegv; break;
        case SIGABRT: old_action = &g_SignalState.old_sigabrt; break;
        case SIGBUS:  old_action = &g_SignalState.old_sigbus; break;
        case SIGILL:  old_action = &g_SignalState.old_sigill; break;
        default: break;
    }

    if (old_action)
    {
        if (old_action->sa_flags & SA_SIGINFO)
        {
            void (*handler)(int, siginfo_t*, void*) = old_action->sa_sigaction;
            if (handler &&
                handler != (void (*)(int, siginfo_t*, void*))SIG_DFL &&
                handler != (void (*)(int, siginfo_t*, void*))SIG_IGN)
            {
                handler(signum, info, context);
            }
        }
        else
        {
            void (*handler)(int) = old_action->sa_handler;
            if (handler && handler != SIG_DFL && handler != SIG_IGN)
            {
                handler(signum);
            }
        }
    }

    signal(signum, SIG_DFL);
    raise(signum);
    _Exit(128 + signum);
}

static bool ShouldInstallSignalHandler()
{
    static int s_cached = -1;
    if (s_cached != -1)
        return s_cached != 0;

    const char* value = getenv("DM_RIVE_ENABLE_SIGNAL_HANDLER");
    if (value == 0 || value[0] == '\0')
    {
        s_cached = 0;
        return false;
    }

    s_cached = (value[0] == '0' && value[1] == '\0') ? 0 : 1;
    return s_cached != 0;
}

static void MaybeOpenSignalDumpFile()
{
    const char* path = getenv("DM_RIVE_SIGNAL_DUMP_PATH");
    if (path == 0 || path[0] == '\0')
        return;

    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1)
        return;

    g_SignalDumpFd = fd;

    char header[256];
    int len = snprintf(header, sizeof(header), "rive_signal_dump v2 pid=%d\n", (int)getpid());
    if (len > 0)
    {
        WriteAll(g_SignalDumpFd, header, (size_t)len);
    }
}
#endif

static void ThrowRuntimeException(JNIEnv* env, const char* message)
{
    if (env == 0)
    {
        return;
    }

    if (env->ExceptionCheck())
    {
        env->ExceptionClear();
    }

    jclass cls = env->FindClass("java/lang/RuntimeException");
    if (cls != 0)
    {
        env->ThrowNew(cls, message ? message : "C++ exception");
        env->DeleteLocalRef(cls);
    }
}

void HandleCppException(JNIEnv* env, const char* context, const char* message)
{
    const char* safe_context = context ? context : "<unknown>";
    const char* safe_message = message ? message : "<no message>";

    printf("C++ exception in %s: %s\n", safe_context, safe_message);
    PrintCallstack();
    fflush(stdout);

    ThrowRuntimeException(env, safe_message);
}

void HandleUnknownCppException(JNIEnv* env, const char* context)
{
    const char* safe_context = context ? context : "<unknown>";
    const char* message = "Unknown C++ exception";

    printf("C++ exception in %s: %s\n", safe_context, message);
    PrintCallstack();
    fflush(stdout);

    ThrowRuntimeException(env, message);
}

#if defined(__APPLE__) || defined(__linux__)
ScopedSignalHandler::ScopedSignalHandler()
{
    m_State.installed = false;
    if (!ShouldInstallSignalHandler())
        return;

    if (g_SignalStateDepth == 0)
    {
        MaybeOpenSignalDumpFile();

        struct sigaction action;
        memset(&action, 0, sizeof(action));
        sigemptyset(&action.sa_mask);
        action.sa_sigaction = SignalHandler;
        action.sa_flags = SA_SIGINFO | SA_RESETHAND;

        sigaction(SIGSEGV, &action, &g_SignalState.old_sigsegv);
        sigaction(SIGABRT, &action, &g_SignalState.old_sigabrt);
        sigaction(SIGBUS, &action, &g_SignalState.old_sigbus);
        sigaction(SIGILL, &action, &g_SignalState.old_sigill);
        g_SignalState.installed = true;
    }
    g_SignalStateDepth++;
    m_State.installed = true;
}

ScopedSignalHandler::~ScopedSignalHandler()
{
    if (!m_State.installed)
        return;

    if (g_SignalStateDepth > 0)
        g_SignalStateDepth--;

    if (g_SignalStateDepth == 0 && g_SignalState.installed)
    {
        sigaction(SIGSEGV, &g_SignalState.old_sigsegv, 0);
        sigaction(SIGABRT, &g_SignalState.old_sigabrt, 0);
        sigaction(SIGBUS, &g_SignalState.old_sigbus, 0);
        sigaction(SIGILL, &g_SignalState.old_sigill, 0);
        g_SignalState.installed = false;
    }
}
#endif

} // namespace dmRiveCrash
