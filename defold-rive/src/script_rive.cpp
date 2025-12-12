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

/**
 * Lua wrapper for pointer movement.
 * @name rive.pointer_move(component, x, y)
 * @param url [type: url] Component receiving the pointer move.
 * @param x [type: number] Pointer x coordinate in component space.
 * @param y [type: number] Pointer y coordinate in component space.
 */
static int Script_PointerMove(lua_State* L)
{
    return Script_PointerAction(L, dmRive::PointerAction::POINTER_MOVE);
}

/**
 * Lua wrapper for pointer up events.
 * @name rive.pointer_up(component, x, y)
 * @param url [type: url] Component receiving the pointer release.
 * @param x [type: number] Pointer x coordinate.
 * @param y [type: number] Pointer y coordinate.
 */
static int Script_PointerUp(lua_State* L)
{
    return Script_PointerAction(L, dmRive::PointerAction::POINTER_UP);
}

/**
 * Lua wrapper for pointer down events.
 * @name rive.pointer_down(component, x, y)
 * @param url [type: url] Component receiving the pointer press.
 * @param x [type: number] Pointer x coordinate.
 * @param y [type: number] Pointer y coordinate.
 */
static int Script_PointerDown(lua_State* L)
{
    return Script_PointerAction(L, dmRive::PointerAction::POINTER_DOWN);
}

/**
 * Lua wrapper for pointer exit events.
 * @name rive.pointer_exit(component, x, y)
 * @param url [type: url] Component receiving the pointer leave.
 * @param x [type: number] Pointer x coordinate.
 * @param y [type: number] Pointer y coordinate.
 */
static int Script_PointerExit(lua_State* L)
{
    return Script_PointerAction(L, dmRive::PointerAction::POINTER_EXIT);
}

/**
 * Returns the projection matrix in render coordinates.
 * @name rive.get_projection_matrix()
 * @return matrix [type: vmath.matrix4] Current projection matrix for the window.
 */
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

/**
 * Sets or clears the global file listener callback.
 * @name rive.set_file_listener(callback)
 * @param callback [type:function(self, event, data)|nil] Callback invoked for file system events; pass nil to disable.
 *
 * `self`
 * : [type:object] The calling script instance.
 *
 * `event`
 * : [type: string] One of:
 *   - `onFileLoaded`
 *   - `onFileDeleted`
 *   - `onFileError`
 *   - `onArtboardsListed`
 *   - `onViewModelsListed`
 *   - `onViewModelInstanceNamesListed`
 *   - `onViewModelPropertiesListed`
 *   - `onViewModelEnumsListed`
 *
 * `data`
 * : [type:table] Additional fields vary by event. Common keys include:
 *   - `file`: [type:handle] File handle involved in the event.
 *   - `viewModelName`: [type:string] View model name for the request, when applicable.
 *   - `instanceNames`: [type:table] Array of instance name strings.
 *   - `artboardNames`: [type:table] Array of artboard name strings.
 *   - `properties`: [type:table] Array of property metadata tables.
 *   - `enums`: [type:table] Array of enum definitions.
 *   - `error`: [type:string] Error message when a failure occurs.
 */
static int Script_SetFileListener(lua_State* L)
{
    return SetListenerCallback(L, &g_FileListener);
}

/**
 * Sets or clears the artboard listener callback.
 * @name rive.set_artboard_listener(callback)
 * @param callback [type:function(self, event, data)|nil] Callback invoked for artboard-related events; pass nil to disable.
 *
 * `self`
 * : [type:object] The calling script instance.
 *
 * `event`
 * : [type: string] One of:
 *   - `onArtboardError`
 *   - `onDefaultViewModelInfoReceived`
 *   - `onArtboardDeleted`
 *   - `onStateMachinesListed`
 *
 * `data`
 * : [type:table] Additional data per event, typically:
 *   - `artboard`: [type:handle] Artboard handle involved.
 *   - `viewModelName`: [type:string] View model name for defaults (received event).
 *   - `instanceName`: [type:string] Instance name for defaults.
 *   - `stateMachineNames`: [type:table] Array of state machine name strings.
 *   - `error`: [type:string] Error message when an error event fires.
 */
static int Script_SetArtboardListener(lua_State* L)
{
    return SetListenerCallback(L, &g_ArtboardListener);
}

/**
 * Sets or clears the state machine listener callback.
 * @name rive.set_state_machine_listener(callback)
 * @param callback [type:function(self, event, data)|nil] Callback invoked for state machine events; pass nil to disable.
 *
 * `self`
 * : [type:object] The calling script instance.
 *
 * `event`
 * : [type: string] One of:
 *   - `onStateMachineError`
 *   - `onStateMachineDeleted`
 *   - `onStateMachineSettled`
 *
 * `data`
 * : [type:table] Event-specific details:
 *   - `stateMachine`: [type:handle] Active state machine handle.
 *   - `error`: [type:string] Error message when an error occurs.
 */
static int Script_SetStateMachineListener(lua_State* L)
{
    return SetListenerCallback(L, &g_StateMachineListener);
}

/**
 * Sets or clears the view model instance listener callback.
 * @name rive.set_view_model_instance_listener(callback)
 * @param callback [type:function(self, event, data)|nil] Callback invoked for view model instance events; pass nil to disable.
 *
 * `self`
 * : [type:object] The calling script instance.
 *
 * `event`
 * : [type: string] One of:
 *   - `onViewModelInstanceError`
 *   - `onViewModelDeleted`
 *   - `onViewModelDataReceived`
 *   - `onViewModelListSizeReceived`
 *
 * `data`
 * : [type:table] Additional payload per event:
 *   - `viewModel`: [type:handle] Handle of the affected view model instance.
 *   - `error`: [type:string] Error description when an error fires.
 *   - `path`: [type:string] Path being inspected when list size arrives.
 *   - `size`: [type:number] List size value for list-size events.
 */
static int Script_SetViewModelInstanceListener(lua_State* L)
{
    return SetListenerCallback(L, &g_ViewModelInstanceListener);
}

/**
 * Sets or clears the render image listener callback.
 * @name rive.set_render_image_listener(callback)
 * @param callback [type:function(self, event, data)|nil] Callback invoked for render image events; pass nil to disable.
 *
 * `self`
 * : [type:object] The calling script instance.
 *
 * `event`
 * : [type: string] One of:
 *   - `onRenderImageDecoded`
 *   - `onRenderImageError`
 *   - `onRenderImageDeleted`
 *
 * `data`
 * : [type:table] Additional fields:
 *   - `renderImage`: [type:handle] Handle of the render image.
 *   - `error`: [type:string] Error message for the failure event.
 */
static int Script_SetRenderImageListener(lua_State* L)
{
    return SetListenerCallback(L, &g_RenderImageListener);
}

/**
 * Sets or clears the audio source listener callback.
 * @name rive.set_audio_source_listener(callback)
 * @param callback [type:function(self, event, data)|nil] Callback invoked for audio source events; pass nil to disable.
 *
 * `self`
 * : [type:object] The calling script instance.
 *
 * `event`
 * : [type: string] One of:
 *   - `onAudioSourceDecoded`
 *   - `onAudioSourceError`
 *   - `onAudioSourceDeleted`
 *
 * `data`
 * : [type:table] Additional fields:
 *   - `audioSource`: [type:handle] Audio source handle for the event.
 *   - `error`: [type:string] Error message when provided.
 */
static int Script_SetAudioSourceListener(lua_State* L)
{
    return SetListenerCallback(L, &g_AudioSourceListener);
}

/**
 * Sets or clears the font listener callback.
 * @name rive.set_font_listener(callback)
 * @param callback [type:function(self, event, data)|nil] Callback invoked for font events; pass nil to disable.
 *
 * `self`
 * : [type:object] The calling script instance.
 *
 * `event`
 * : [type: string] One of:
 *   - `onFontDecoded`
 *   - `onFontError`
 *   - `onFontDeleted`
 *
 * `data`
 * : [type:table] Additional fields:
 *   - `font`: [type:handle] Font handle for the associated event.
 *   - `error`: [type:string] Error message for failure events.
 */
static int Script_SetFontListener(lua_State* L)
{
    return SetListenerCallback(L, &g_FontListener);
}

// ****************************************************************************

/**
 * Returns the Rive file handle tied to the component.
 * @name rive.get_file(component)
 * @param url [type: url] Component whose file handle to query.
 * @return file_handle [type: FileHandle] Handle identifying the loaded Rive file.
 */
static int Script_GetFile(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
    lua_pushinteger(L, (lua_Integer) CompRiveGetFile(component));
    return 1;
}

/**
 * Switches the active artboard for the component.
 * @name rive.set_artboard(component, name)
 * @param url [type: url] Component using the artboard.
 * @param name [type: string|nil] Name of the artboard to create and set. Pass nil to create a default artboard.
 * @return artboard [type: ArtboardHandle] Old artboard handle
 */
static int Script_SetArtboard(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
    const char* name;
    if (lua_isnil(L, 2))
        name = 0;
    else
        name = luaL_checkstring(L, 2);
    rive::ArtboardHandle old_handle = CompRiveSetArtboard(component, name);
    lua_pushinteger(L, (lua_Integer) old_handle);
    return 1;
}

/**
 * Queries the current artboard handle for the component.
 * @name rive.get_artboard(component)
 * @param url [type: url] Component whose artboard handle to return.
 * @return artboard [type: ArtboardHandle] Active artboard handle.
 */
static int Script_GetArtboard(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
    lua_pushinteger(L, (lua_Integer) CompRiveGetArtboard(component));
    return 1;
}

/**
 * Selects a state machine by name on the component.
 * @name rive.set_state_machine(component, name)
 * @param url [type: url] Component owning the state machine.
 * @param name [type: string|nil] Name of the state machine to create and set. Pass nil to create a default state machine.
 * @return state_machine [type: StateMachineHandle] Old state machine handle
 */
static int Script_SetStateMachine(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
    const char* name;
    if (lua_isnil(L, 2))
        name = 0;
    else
        name = luaL_checkstring(L, 2);
    rive::StateMachineHandle old_handle = CompRiveSetStateMachine(component, name);
    lua_pushinteger(L, (lua_Integer) old_handle);
    return 1;
}

/**
 * Returns the active state machine handle for the component.
 * @name rive.get_state_machine(component)
 * @param url [type: url] Component whose active state machine to query.
 * @return state_machine [type: StateMachineHandle] Current state machine handle.
 */
static int Script_GetStateMachine(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
    lua_pushinteger(L, (lua_Integer) CompRiveGetStateMachine(component));
    return 1;
}

/**
 * Selects a view model instance by name.
 * @name rive.set_view_model_instance(component, name)
 * @param url [type: url] Component owning the view model instance.
 * @param name [type: string] View model instance name to activate.
 * @return success [type: boolean] True if the view model instance was activated.
 */
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

/**
 * Returns the handle of the currently bound view model instance.
 * @name rive.get_view_model_instance(component)
 * @param url [type: url] Component whose view model instance handle to query.
 * @return view_model_instance_handle [type: ViewModelInstanceHandle] Handle for the active view model instance.
 */
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
