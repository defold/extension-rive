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

#include <stdint.h>

#include <defold/renderer.h>

#include <rive/animation/state_machine_instance.hpp>
#include <rive/refcnt.hpp>

#include <rive/command_queue.hpp>

namespace rive
{
    class Factory;
}

namespace dmRiveCommands
{
    enum Result
    {
        RESULT_OK = 0,
        RESULT_FAILED_CREATE_THREAD = -1,
    };

    struct InitParams
    {
        dmRive::HRenderContext  m_RenderContext;
        bool                    m_UseThreads;

        InitParams()
        : m_RenderContext(0)
        , m_UseThreads(true)
        {}
    };

    Result Initialize(InitParams* params); // Once per session
    Result Finalize();   // Once per session

    Result ProcessMessages();

    // Getters
    rive::Factory*                  GetFactory();
    dmRive::HRenderContext          GetDefoldRenderContext();
    rive::rcp<rive::CommandQueue>   GetCommandQueue();
}
