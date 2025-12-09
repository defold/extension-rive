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

#include <dmsdk/dlib/atomic.h>
#include <dmsdk/dlib/mutex.h>
#include <dmsdk/dlib/thread.h>

#include <rive/artboard.hpp>
#include <rive/factory.hpp>
#include <rive/refcnt.hpp>

#include <rive/command_queue.hpp>
#include <rive/command_server.hpp>


namespace dmRiveCommands {

struct Context
{
    dmThread::Thread    m_Thread;
    int32_atomic_t      m_Run;

    dmRive::HRenderContext          m_RenderContext;
    rive::Factory*                  m_Factory;
    rive::CommandServer*            m_CommandServer;
    rive::rcp<rive::CommandQueue>   m_CommandQueue;
};

Context* g_Context = 0;

static void RiveCommandThread(void* _ctx)
{
    Context* ctx = (Context*)_ctx;

    while (dmAtomicGet32(&g_Context->m_Run))
    {

    }
}

Result Initialize(InitParams* params)
{
    assert(g_Context == 0);

    g_Context = new Context;
    memset(g_Context, 0, sizeof(*g_Context));

    if (params->m_UseThreads)
    {
        dmAtomicAdd32(&g_Context->m_Run, 1);
        g_Context->m_Thread = dmThread::New(RiveCommandThread, 1 * 1024*1024, g_Context, "RiveCommandThread");
        if (!g_Context->m_Thread)
        {
            return RESULT_FAILED_CREATE_THREAD;
        }
    }

    g_Context->m_RenderContext = params->m_RenderContext;
    g_Context->m_Factory = dmRive::GetRiveFactory(g_Context->m_RenderContext);

    g_Context->m_CommandQueue = rive::make_rcp<rive::CommandQueue>();
    g_Context->m_CommandServer = new rive::CommandServer(g_Context->m_CommandQueue, g_Context->m_Factory);

    return RESULT_OK;
}

Result Finalize()
{
    assert(g_Context != 0);

    if (dmAtomicGet32(&g_Context->m_Run))
    {
        dmAtomicSub32(&g_Context->m_Run, 1);
        dmThread::Join(g_Context->m_Thread);
        g_Context->m_Thread = 0;
    }

    g_Context->m_CommandQueue.reset();
    delete g_Context->m_CommandServer;

    delete g_Context;
    g_Context = 0;
    return RESULT_OK;
}

rive::rcp<rive::CommandQueue> GetCommandQueue()
{
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
    printf("GetDefoldRenderContext: %p\n", g_Context->m_RenderContext);
    return g_Context->m_RenderContext;
}

Result ProcessMessages()
{
    assert(g_Context != 0);
    g_Context->m_CommandServer->processCommands();
    g_Context->m_CommandQueue->processMessages();
    return RESULT_OK;
}


} // namespace
