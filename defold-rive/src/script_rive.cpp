// Copyright 2021 The Defold Foundation
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

#if !defined(DM_RIVE_UNSUPPORTED)

/*# Rive model API documentation
 *
 * Functions and messages for interacting with the 'Rive'
 * animation system.
 *
 * @document
 * @name Rive
 * @namespace rive
 */

#include <dmsdk/sdk.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/message.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/gameobject/script.h>
#include <dmsdk/gamesys/script.h>
#include <dmsdk/resource/resource.h>
#include <dmsdk/graphics/graphics.h>

#include "comp_rive.h"
#include "comp_rive_private.h"
#include "rive_ddf.h"
#include "res_rive_data.h"

#include "script_rive.h"
#include "script_rive_cmd.h"
#include "script_rive_listeners.h"

#include <common/commands.h>

namespace dmRive
{
static const char*    RIVE_EXT      = "rivc";
static const dmhash_t RIVE_EXT_HASH = dmHashString64(RIVE_EXT);

static dmResource::HFactory g_Factory = 0;

FileListener                g_FileListener;
ArtboardListener            g_ArtboardListener;
StateMachineListener        g_StateMachineListener;
ViewModelInstanceListener   g_ViewModelInstanceListener;
RenderImageListener         g_RenderImageListener;
AudioSourceListener         g_AudioSourceListener;
FontListener                g_FontListener;

static int Script_PointerAction(lua_State* L, dmRive::PointerAction action)
{
    DM_LUA_STACK_CHECK(L, 0);

    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
    lua_Number x = luaL_checknumber(L, 2);
    lua_Number y = luaL_checknumber(L, 3);

    CompRivePointerAction(component, action, x, y);
    return 0;
}

static int Script_PointerMove(lua_State* L)
{
    return Script_PointerAction(L, dmRive::PointerAction::POINTER_MOVE);
}

static int Script_PointerUp(lua_State* L)
{
    return Script_PointerAction(L, dmRive::PointerAction::POINTER_UP);
}

static int Script_PointerDown(lua_State* L)
{
    return Script_PointerAction(L, dmRive::PointerAction::POINTER_DOWN);
}

static int Script_PointerExit(lua_State* L)
{
    return Script_PointerAction(L, dmRive::PointerAction::POINTER_EXIT);
}

static int Script_GetProjectionMatrix(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    dmGraphics::HContext graphics_context = dmGraphics::GetInstalledContext();

    float scale_factor = CompRiveGetDisplayScaleFactor();
    float right = (float) dmGraphics::GetWindowWidth(graphics_context) / scale_factor;
    float top = (float) dmGraphics::GetWindowHeight(graphics_context) / scale_factor;

    dmScript::PushMatrix4(L, dmVMath::Matrix4::orthographic(0, right, 0, top, -1, 1));

    return 1;
}

// This is an "all bets are off" mode.
static int Script_DebugSetBlitMode(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (!lua_isboolean(L, 1))
    {
        return DM_LUA_ERROR("The first argument must be a boolean.");
    }
    bool value = lua_toboolean(L, 1);
    CompRiveDebugSetBlitMode(value);
    return 0;
}

// ****************************************************************************

template<typename LISTENER>
static int SetListenerCallback(lua_State* L, LISTENER* listener)
{
    if (listener->m_Callback)
        dmScript::DestroyCallback(listener->m_Callback);
    listener->m_Callback = 0;

    if (lua_isnil(L, 1))
    {
        return 0;
    }

    listener->m_Callback = dmScript::CreateCallback(L, 1);

    if (!dmScript::IsCallbackValid(listener->m_Callback))
    {
        listener->m_Callback = 0;
        return luaL_error(L, "Failed to create callback");
    }
    return 0;
}

static int Script_SetFileListener(lua_State* L)
{
    return SetListenerCallback(L, &g_FileListener);
}

static int Script_SetArtboardListener(lua_State* L)
{
    return SetListenerCallback(L, &g_ArtboardListener);
}

static int Script_SetStateMachineListener(lua_State* L)
{
    return SetListenerCallback(L, &g_StateMachineListener);
}

static int Script_SetViewModelInstanceListener(lua_State* L)
{
    return SetListenerCallback(L, &g_ViewModelInstanceListener);
}

static int Script_SetRenderImageListener(lua_State* L)
{
    return SetListenerCallback(L, &g_RenderImageListener);
}

static int Script_SetAudioSourceListener(lua_State* L)
{
    return SetListenerCallback(L, &g_AudioSourceListener);
}

static int Script_SetFontListener(lua_State* L)
{
    return SetListenerCallback(L, &g_FontListener);
}

// ****************************************************************************

static int Script_GetFile(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
    lua_pushinteger(L, (lua_Integer) CompRiveGetFile(component));
    return 1;
}

static int Script_SetArtboard(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
    const char* name = luaL_checkstring(L, 2);
    bool result = CompRiveSetArtboard(component, name);
    // TODO: Allow it to set a new statemachine at the same time!
    lua_pushboolean(L, result);
    return 1;
}

static int Script_GetArtboard(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
    lua_pushinteger(L, (lua_Integer) CompRiveGetArtboard(component));
    return 1;
}

static int Script_SetStateMachine(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
    const char* name = luaL_checkstring(L, 2);
    bool result = CompRiveSetStateMachine(component, name);
    lua_pushboolean(L, result);
    return 1;
}

static int Script_GetStateMachine(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
    lua_pushinteger(L, (lua_Integer) CompRiveGetStateMachine(component));
    return 1;
}

static int Script_SetViewModelInstance(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
    const char* name = luaL_checkstring(L, 2);
    bool result = CompRiveSetViewModelInstance(component, name);
    lua_pushboolean(L, result);
    return 1;
}

static int Script_GetViewModelInstance(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
    lua_pushinteger(L, (lua_Integer) CompRiveGetStateMachine(component));
    return 1;
}


static const luaL_reg RIVE_FUNCTIONS[] =
{
    {"pointer_move",            Script_PointerMove},
    {"pointer_up",              Script_PointerUp},
    {"pointer_down",            Script_PointerDown},
    {"pointer_exit",            Script_PointerExit},
    {"get_projection_matrix",   Script_GetProjectionMatrix},

    {"set_file_listener",               Script_SetFileListener},
    {"set_artboard_listener",           Script_SetArtboardListener},
    {"set_state_machine_listener",      Script_SetStateMachineListener},
    {"set_view_model_instance_listener",Script_SetViewModelInstanceListener},
    {"set_render_image_listener",       Script_SetRenderImageListener},
    {"set_audio_source_listener",       Script_SetAudioSourceListener},
    {"set_font_listener",               Script_SetFontListener},

    {"get_file",                Script_GetFile},
    {"set_artboard",            Script_SetArtboard},
    {"get_artboard",            Script_GetArtboard},
    {"set_state_machine",       Script_SetStateMachine},
    {"get_state_machine",       Script_GetStateMachine},
    {"set_view_model_instance", Script_SetViewModelInstance},
    {"get_view_model_instance", Script_GetViewModelInstance},

    // debug
    {"debug_set_blit_mode",     Script_DebugSetBlitMode},

    {0, 0}
};

void ScriptRegister(lua_State* L, dmResource::HFactory factory)
{
    // Setup the global listeners
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->setGlobalFileListener(&g_FileListener);
    queue->setGlobalArtboardListener(&g_ArtboardListener);
    queue->setGlobalStateMachineListener(&g_StateMachineListener);
    queue->setGlobalViewModelInstanceListener(&g_ViewModelInstanceListener);
    queue->setGlobalRenderImageListener(&g_RenderImageListener);
    queue->setGlobalAudioSourceListener(&g_AudioSourceListener);
    queue->setGlobalFontListener(&g_FontListener);

    luaL_register(L, "rive", RIVE_FUNCTIONS);
        ScriptCmdRegister(L, factory);
    lua_pop(L, 1);

    g_Factory = factory;
}

void ScriptUnregister(lua_State* L, dmResource::HFactory factory)
{
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->setGlobalFileListener(0);
    queue->setGlobalArtboardListener(0);
    queue->setGlobalStateMachineListener(0);
    queue->setGlobalViewModelInstanceListener(0);
    queue->setGlobalRenderImageListener(0);
    queue->setGlobalAudioSourceListener(0);
    queue->setGlobalFontListener(0);

    ScriptCmdUnregister(L, factory);
    g_Factory = 0;
}

}

#endif // DM_RIVE_UNSUPPORTED
