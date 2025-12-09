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

#if defined(DM_UNSUPPORTED_PLATFORM)
    #error "This platform is not supported"
#endif

#include <dmsdk/sdk.h>
#include <dmsdk/extension/extension.h>
#include <dmsdk/resource/resource.h>

#include "script_rive.h"
#include <defold/rive_version.h>
#include "defold/renderer.h"

#include <common/commands.h>

dmRive::HRenderContext g_RenderContext = 0;

static dmExtension::Result AppInitializeRive(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeRive(dmExtension::Params* params)
{
    g_RenderContext = dmRive::NewRenderContext();
    assert(g_RenderContext != 0);

    dmRiveCommands::InitParams cmd_params;
    cmd_params.m_UseThreads = true; // TODO: Use define and/or config flag
    cmd_params.m_RenderContext = g_RenderContext;
    dmRiveCommands::Initialize(&cmd_params);

    // relies on the command queue for registering listeners
    dmResource::HFactory factory = dmExtension::GetContextAsType<dmResource::HFactory>(params, "factory");
    dmRive::ScriptRegister(params->m_L, factory);

    dmLogInfo("Registered Rive extension:  %s  %s\n", RIVE_RUNTIME_DATE, RIVE_RUNTIME_SHA1);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeRive(dmExtension::Params* params)
{
    dmResource::HFactory factory = dmExtension::GetContextAsType<dmResource::HFactory>(params, "factory");
    dmRive::ScriptUnregister(params->m_L, factory);

    dmRiveCommands::Finalize();

    dmRive::DeleteRenderContext(g_RenderContext);
    g_RenderContext = 0;

    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeRive(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(RiveExt, "RiveExt", AppInitializeRive, AppFinalizeRive, InitializeRive, 0, 0, FinalizeRive);
