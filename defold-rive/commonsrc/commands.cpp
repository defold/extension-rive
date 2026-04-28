// Copyright 2020-2025 The Defold Foundation
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

#include <common/commands.h>

#include <stdint.h>

#include <dmsdk/dlib/atomic.h>
#include <dmsdk/dlib/mutex.h>
#include <dmsdk/dlib/thread.h>
#include <dmsdk/dlib/time.h>

#include <rive/artboard.hpp>
#include <rive/factory.hpp>
#include <rive/file.hpp>
#include <rive/refcnt.hpp>
#include <rive/scripted/scripted_object.hpp>

#include <rive/command_queue.hpp>
#include <rive/command_server.hpp>

namespace dmRiveCommands {

struct Context
{
    dmThread::Thread    m_Thread;
    int32_atomic_t      m_Run;
    dmMutex::HMutex     m_Mutex;

    dmRive::HRenderContext          m_RenderContext;
    rive::Factory*                  m_Factory;
    rive::CommandServer*            m_CommandServer;
    rive::rcp<rive::CommandQueue>   m_CommandQueue;
};

Context* g_Context = 0;

static void DestroyContext(Context* context)
{
    if (context == 0)
    {
        return;
    }

    if (context->m_Thread)
    {
        dmAtomicStore32(&context->m_Run, 0);
        if (context->m_CommandQueue)
        {
            context->m_CommandQueue->disconnect();
        }
        dmThread::Join(context->m_Thread);
        context->m_Thread = 0;
    }

    context->m_CommandQueue.reset();

    if (context->m_CommandServer != 0)
    {
        delete context->m_CommandServer;
        context->m_CommandServer = 0;
    }

    context->m_Mutex = 0;

    delete context;
}

static void PumpMessagesLocked(Context* context)
{
    if (!context->m_Thread)
    {
        context->m_CommandServer->processCommands();
    }

    context->m_CommandQueue->processMessages();
}

static void PumpMessages(Context* context)
{
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_Mutex);
    PumpMessagesLocked(context);
}

template <typename Fn>
static bool RunOnServerAndWait(Context* context, Fn fn)
{
    assert(context != 0);
    assert(context->m_CommandQueue);

    int32_atomic_t done = 0;
    context->m_CommandQueue->runOnce([&done, fn](rive::CommandServer* server) mutable {
        fn(server);
        dmAtomicStore32(&done, 1);
    });

    while (dmAtomicGet32(&done) == 0)
    {
        PumpMessages(context);
        if (dmAtomicGet32(&done) != 0)
        {
            break;
        }

        dmTime::Sleep(1000);
    }

    // Ensure listener callbacks produced by the fenced work are delivered.
    PumpMessages(context);
    return true;
}

static void DisposeArtboardScripts(rive::Artboard* artboard)
{
    if (artboard == 0)
    {
        return;
    }

    size_t object_count = artboard->objects().size();
    for (size_t i = 0; i < object_count; ++i)
    {
        rive::Core* object = artboard->objects()[i];
        rive::ScriptInput* script_input = rive::ScriptInput::from(object);
        if (script_input != 0)
        {
            script_input->scriptedObject(0);
        }
    }

    for (size_t i = 0; i < object_count; ++i)
    {
        rive::Core* object = artboard->objects()[i];
        rive::ScriptedObject* scripted_object = rive::ScriptedObject::from(object);
        if (scripted_object != 0)
        {
            scripted_object->scriptDispose();
        }
    }
}

static void RiveCommandThread(void* _ctx)
{
    Context* ctx = (Context*)_ctx;
    ctx->m_CommandServer = new rive::CommandServer(ctx->m_CommandQueue, ctx->m_Factory);

    while (dmAtomicGet32(&ctx->m_Run))
    {
        bool keep_running = true;
        {
            // This lock is due to RenderContext interactions
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(ctx->m_Mutex);
            keep_running = ctx->m_CommandServer->processCommands();
        }

        if (keep_running == false)
        {
            break;
        }

        dmTime::Sleep(1);
    }
}

Result Initialize(InitParams* params)
{
    assert(g_Context == 0);

    g_Context = new Context;
    memset(g_Context, 0, sizeof(*g_Context));
    g_Context->m_Mutex = params->m_Mutex;

    g_Context->m_RenderContext = params->m_RenderContext;
    g_Context->m_Factory = params->m_Factory;

    g_Context->m_CommandQueue = rive::make_rcp<rive::CommandQueue>();
    if (params->m_UseThreads == false)
    {
        g_Context->m_CommandServer = new rive::CommandServer(g_Context->m_CommandQueue, g_Context->m_Factory);
    }

    if (params->m_UseThreads)
    {
        dmAtomicAdd32(&g_Context->m_Run, 1);
        g_Context->m_Thread = dmThread::New(RiveCommandThread, 1 * 1024 * 1024, g_Context, "RiveCommandThread");
        if (!g_Context->m_Thread)
        {
            DestroyContext(g_Context);
            g_Context = 0;
            return RESULT_FAILED_CREATE_THREAD;
        }
    }

    return RESULT_OK;
}

Result Finalize()
{
    assert(g_Context != 0);
    DestroyContext(g_Context);
    g_Context = 0;
    return RESULT_OK;
}

rive::rcp<rive::CommandQueue> GetCommandQueue()
{
    assert(g_Context != 0);
    return g_Context->m_CommandQueue;
}

rive::Factory* GetFactory()
{
    assert(g_Context != 0);
    return g_Context->m_Factory;
}

dmRive::HRenderContext GetDefoldRenderContext()
{
    assert(g_Context != 0);
    return g_Context->m_RenderContext;
}

Result ProcessMessages()
{
    assert(g_Context != 0);
    if (g_Context->m_Thread)
    {
        RunOnServerAndWait(g_Context, [](rive::CommandServer*) {});
    }
    else
    {
        PumpMessages(g_Context);
    }
    return RESULT_OK;
}

bool WaitUntil(bool (*condition)(void*), void* user_data, uint64_t timeout)
{
    assert(g_Context != 0);
    assert(condition != 0);

    uint64_t deadline = UINT64_MAX;
    if (timeout != 0)
    {
        deadline = dmTime::GetMonotonicTime() + timeout;
    }

    do
    {
        if (condition(user_data))
        {
            return true;
        }

        if (dmTime::GetMonotonicTime() >= deadline)
        {
            return false;
        }

        ProcessMessages();
    } while (true);
}

bool GetBounds(rive::ArtboardHandle artboard_handle, rive::AABB* out_bounds)
{
    if (g_Context == 0 || artboard_handle == RIVE_NULL_HANDLE || out_bounds == 0)
    {
        return false;
    }

    bool found = false;
    rive::AABB bounds;
    bool completed = RunOnServerAndWait(g_Context, [&](rive::CommandServer* server) {
        rive::ArtboardInstance* artboard = server->getArtboardInstance(artboard_handle);
        if (artboard != 0)
        {
            bounds = artboard->bounds();
            found = true;
        }
    });

    if (!completed || !found)
    {
        return false;
    }

    *out_bounds = bounds;
    return true;
}

bool DisposeArtboardScripts(rive::ArtboardHandle artboard_handle)
{
    if (g_Context == 0 || artboard_handle == RIVE_NULL_HANDLE)
    {
        return false;
    }

    bool found = false;
    bool completed = RunOnServerAndWait(g_Context, [&](rive::CommandServer* server) {
        rive::ArtboardInstance* artboard = server->getArtboardInstance(artboard_handle);
        if (artboard == 0)
        {
            return;
        }

        found = true;
        DisposeArtboardScripts(artboard);
    });

    return completed && found;
}

bool DisposeFileScripts(rive::FileHandle file_handle)
{
    if (g_Context == 0 || file_handle == RIVE_NULL_HANDLE)
    {
        return false;
    }

    bool found = false;
    bool completed = RunOnServerAndWait(g_Context, [&](rive::CommandServer* server) {
        rive::File* file = server->getFile(file_handle);
        if (file == 0)
        {
            return;
        }

        found = true;
        for (size_t i = 0; i < file->artboardCount(); ++i)
        {
            DisposeArtboardScripts(file->artboard(i));
        }
    });

    return completed && found;
}

} // namespace
