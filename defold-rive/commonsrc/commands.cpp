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

#include <assert.h>
#include <stdint.h>

#include <dmsdk/dlib/atomic.h>
#include <dmsdk/dlib/mutex.h>
#include <dmsdk/dlib/thread.h>
#include <dmsdk/dlib/time.h>

#include <rive/artboard.hpp>
#include <rive/assets/file_asset.hpp>
#include <rive/factory.hpp>
#include <rive/file.hpp>
#include <rive/refcnt.hpp>
#include <rive/script_input_artboard.hpp>
#include <rive/script_input_boolean.hpp>
#include <rive/script_input_color.hpp>
#include <rive/script_input_number.hpp>
#include <rive/script_input_string.hpp>
#include <rive/script_input_trigger.hpp>
#include <rive/script_input_viewmodel_property.hpp>
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
    bool                            m_UseThreads;
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
    if (context->m_Factory == 0)
    {
        return;
    }

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
static bool RunOnServerAndWait(Context* context, Fn fn, bool dispatch_messages = true)
{
    assert(context != 0);
    assert(context->m_CommandQueue);

    if (context->m_Factory == 0)
    {
        return false;
    }

    int32_atomic_t done = 0;
    context->m_CommandQueue->runOnce([&done, fn](rive::CommandServer* server) mutable {
        fn(server);
        dmAtomicStore32(&done, 1);
    });

    while (dmAtomicGet32(&done) == 0)
    {
        if (dispatch_messages)
        {
            PumpMessages(context);
        }
        else if (!context->m_Thread)
        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_Mutex);
            context->m_CommandServer->processCommands();
        }
        if (dmAtomicGet32(&done) != 0)
        {
            break;
        }

        dmTime::Sleep(1000);
    }

    // Ensure listener callbacks produced by the fenced work are delivered.
    if (dispatch_messages)
    {
        PumpMessages(context);
    }
    return true;
}

static rive::ScriptInput* GetScriptInput(rive::Core* object)
{
    if (object == 0)
    {
        return 0;
    }

    switch (object->coreType())
    {
        case rive::ScriptInputArtboard::typeKey:
            return object->as<rive::ScriptInputArtboard>();
        case rive::ScriptInputBoolean::typeKey:
            return object->as<rive::ScriptInputBoolean>();
        case rive::ScriptInputColor::typeKey:
            return object->as<rive::ScriptInputColor>();
        case rive::ScriptInputNumber::typeKey:
            return object->as<rive::ScriptInputNumber>();
        case rive::ScriptInputString::typeKey:
            return object->as<rive::ScriptInputString>();
        case rive::ScriptInputTrigger::typeKey:
            return object->as<rive::ScriptInputTrigger>();
        case rive::ScriptInputViewModelProperty::typeKey:
            return object->as<rive::ScriptInputViewModelProperty>();
        default:
            return 0;
    }
}

static rive::ScriptedObject* GetScriptedObject(rive::Core* object)
{
    if (object == 0)
    {
        return 0;
    }

    return rive::ScriptedObject::from(object);
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
        rive::ScriptInput* script_input = GetScriptInput(object);
        if (script_input != 0)
        {
            script_input->scriptedObject(0);
        }
    }

    for (size_t i = 0; i < object_count; ++i)
    {
        rive::Core* object = artboard->objects()[i];
        rive::ScriptedObject* scripted_object = GetScriptedObject(object);
        if (scripted_object != 0)
        {
            scripted_object->scriptDispose();
        }
    }
}

static void RiveCommandThread(void* _ctx)
{
    Context* ctx = (Context*)_ctx;
    // Rive records the constructing thread and requires commands to run on that thread.
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

    g_Context = new Context();
    g_Context->m_Mutex = params->m_Mutex;
    g_Context->m_Thread = 0;
    g_Context->m_Run = 0;
    g_Context->m_CommandServer = 0;
    g_Context->m_UseThreads = params->m_UseThreads;

    g_Context->m_RenderContext = params->m_RenderContext;
    g_Context->m_Factory = 0;

    g_Context->m_CommandQueue = rive::make_rcp<rive::CommandQueue>();

    if (params->m_Factory != 0)
    {
        Result result = SetFactory(params->m_Factory);
        if (result != RESULT_OK)
        {
            DestroyContext(g_Context);
            g_Context = 0;
            return result;
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

Result SetFactory(rive::Factory* factory)
{
    assert(g_Context != 0);
    if (factory == 0)
    {
        return RESULT_INVALID_FACTORY;
    }

    if (g_Context->m_Factory != 0)
    {
        return g_Context->m_Factory == factory ? RESULT_OK : RESULT_FACTORY_ALREADY_SET;
    }

    g_Context->m_Factory = factory;
    if (g_Context->m_UseThreads)
    {
        dmAtomicStore32(&g_Context->m_Run, 1);
        g_Context->m_Thread = dmThread::New(RiveCommandThread, 1 * 1024 * 1024, g_Context, "RiveCommandThread");
        if (!g_Context->m_Thread)
        {
            g_Context->m_Factory = 0;
            dmAtomicStore32(&g_Context->m_Run, 0);
            return RESULT_FAILED_CREATE_THREAD;
        }
    }
    else
    {
        g_Context->m_CommandServer = new rive::CommandServer(g_Context->m_CommandQueue, factory);
        if (g_Context->m_CommandServer == 0)
        {
            g_Context->m_Factory = 0;
            return RESULT_FAILED_CREATE_COMMAND_SERVER;
        }
    }

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

bool ArtboardHasDefaultViewModel(rive::FileHandle file_handle, rive::ArtboardHandle artboard_handle)
{
    if (g_Context == 0 || file_handle == RIVE_NULL_HANDLE || artboard_handle == RIVE_NULL_HANDLE)
    {
        return false;
    }

    bool has_view_model = false;
    // Component creation precedes script initialization. Leave callbacks queued until scripts can register listeners.
    bool completed = RunOnServerAndWait(g_Context, [&](rive::CommandServer* server) {
        rive::File* file = server->getFile(file_handle);
        rive::ArtboardInstance* artboard = server->getArtboardInstance(artboard_handle);
        if (file != 0 && artboard != 0)
        {
            // Inspect the binding without creating a ViewModelRuntime or reporting an error for an unbound artboard.
            has_view_model = artboard->viewModelId() < file->viewModelCount();
        }
    }, false);

    return completed && has_view_model;
}

bool FileHasAssetType(rive::FileHandle file_handle, uint16_t type_key, bool* out_has_asset)
{
    if (out_has_asset != 0)
    {
        *out_has_asset = false;
    }

    if (g_Context == 0 || file_handle == RIVE_NULL_HANDLE || out_has_asset == 0)
    {
        return false;
    }

    bool found = false;
    bool has_asset = false;
    bool completed = RunOnServerAndWait(g_Context, [&](rive::CommandServer* server) {
        rive::File* file = server->getFile(file_handle);
        if (file == 0)
        {
            return;
        }

        found = true;
        rive::Span<const rive::rcp<rive::FileAsset> > assets = file->assets();
        for (size_t i = 0; i < assets.size(); ++i)
        {
            rive::FileAsset* asset = assets[i].get();
            if (asset != 0 && asset->isTypeOf(type_key))
            {
                has_asset = true;
                break;
            }
        }
    });

    if (!completed || !found)
    {
        return false;
    }

    *out_has_asset = has_asset;
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
