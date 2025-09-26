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

#include <rive/animation/linear_animation.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include <rive/animation/state_machine.hpp>
#include <rive/animation/state_machine_instance.hpp>

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
#include "script_rive_private.h"


namespace dmRive
{
    static const char*    RIVE_EXT      = "rivc";
    static const dmhash_t RIVE_EXT_HASH = dmHashString64(RIVE_EXT);

    static dmResource::HFactory g_Factory = 0;

    /*# Rive model API documentation
     *
     * Functions and messages for interacting with the 'Rive'
     * animation system.
     *
     * @document
     * @name Rive
     * @namespace rive
     */

    /*# play an animation on a rive model
     * Plays a specified animation on a rive model component with specified playback
     * mode and parameters.
     */
    static int RiveComp_PlayAnim(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        int top = lua_gettop(L);

        RiveComponent* component = 0;
        dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);

        dmRiveDDF::RivePlayAnimation ddf;
        ddf.m_AnimationId       = dmScript::CheckHashOrString(L, 2);;
        ddf.m_Playback          = luaL_checkinteger(L, 3);
        ddf.m_Offset            = 0.0;
        ddf.m_PlaybackRate      = 1.0;
        ddf.m_IsStateMachine    = false;

        if (top > 3) // table with args
        {
            if (lua_istable(L, 4))
            {
                luaL_checktype(L, 4, LUA_TTABLE);
                lua_pushvalue(L, 4);

                lua_getfield(L, -1, "offset");
                ddf.m_Offset = lua_isnil(L, -1) ? 0.0 : luaL_checknumber(L, -1);
                lua_pop(L, 1);

                lua_getfield(L, -1, "playback_rate");
                ddf.m_PlaybackRate = lua_isnil(L, -1) ? 1.0 : luaL_checknumber(L, -1);
                lua_pop(L, 1);

                lua_pop(L, 1);
            }
        }

        dmScript::LuaCallbackInfo* cbk = 0x0;
        if (top > 4) // completed cb
        {
            if (lua_isfunction(L, 5))
            {
                cbk = dmScript::CreateCallback(L, 5);
            }
        }

        if (!CompRivePlayAnimation(component, &ddf, cbk))
        {
            if (cbk)
            {
                dmScript::DestroyCallback(cbk);
            }
        }

        return 0;
    }

    /*# play a state machine on a rive model
     * Plays a specified state machine on a rive model component with specified playback
     * mode and parameters.
     */
    static int RiveComp_PlayStateMachine(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        int top = lua_gettop(L);

        RiveComponent* component = 0;
        dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);

        dmRiveDDF::RivePlayAnimation ddf;
        ddf.m_AnimationId       = dmScript::CheckHashOrString(L, 2);
        ddf.m_Playback          = 0;
        ddf.m_Offset            = 0.0;
        ddf.m_PlaybackRate      = 1.0;
        ddf.m_IsStateMachine    = true;

        if (top > 2) // table with args
        {
            if (lua_istable(L, 3))
            {
                luaL_checktype(L, 3, LUA_TTABLE);
                lua_pushvalue(L, 3);

                lua_getfield(L, -1, "offset");
                ddf.m_Offset = lua_isnil(L, -1) ? 0.0 : luaL_checknumber(L, -1);
                lua_pop(L, 1);

                lua_getfield(L, -1, "playback_rate");
                ddf.m_PlaybackRate = lua_isnil(L, -1) ? 1.0 : luaL_checknumber(L, -1);
                lua_pop(L, 1);

                lua_pop(L, 1);
            }
        }

        dmScript::LuaCallbackInfo* cbk = 0x0;
        if (top > 3) // completed cb
        {
            if (lua_isfunction(L, 4))
            {
                cbk = dmScript::CreateCallback(L, 4);
            }
        }

        if (!CompRivePlayStateMachine(component, &ddf, cbk))
        {
            if (cbk)
            {
                dmScript::DestroyCallback(cbk);
            }
        }

        return 0;
    }

    // Can cancel both a single animation and a state machine playing
    static int RiveComp_Cancel(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        dmGameObject::HInstance instance = dmScript::CheckGOInstance(L);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmRiveDDF::RiveCancelAnimation msg;
        dmMessage::Post(&sender, &receiver, dmRiveDDF::RiveCancelAnimation::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, 0, (uintptr_t)dmRiveDDF::RiveCancelAnimation::m_DDFDescriptor, &msg, sizeof(msg), 0);
        return 0;
    }

    /*# retrieve the game object corresponding to a rive artboard skeleton bone
     * Returns the id of the game object that corresponds to a specified skeleton bone.
     * The returned game object can be used for parenting and transform queries.
     * This function has complexity `O(n)`, where `n` is the number of bones in the rive model skeleton.
     * Game objects corresponding to a rive model skeleton bone can not be individually deleted.
     *
     * @name rive.get_go
     * @param url [type:string|hash|url] the rive model to query
     * @param bone_id [type:string|hash] id of the corresponding bone
     * @return id [type:hash] id of the game object
     * @examples
     *
     * The following examples assumes that the rive model has id "rivemodel".
     *
     * How to parent the game object of the calling script to the "right_hand" bone of the rive model in a player game object:
     *
     * ```lua
     * function init(self)
     *   local parent = rive.get_go("player#rivemodel", "right_hand")
     *   msg.post(".", "set_parent", {parent_id = parent})
     * end
     * ```
     */
    static int RiveComp_GetGO(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        RiveComponent* component = 0;
        dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);

        dmhash_t bone_name = dmScript::CheckHashOrString(L, 2);

        dmhash_t instance_id = 0;
        if (!CompRiveGetBoneID(component, bone_name, &instance_id)) {
            return DM_LUA_ERROR("the bone '%s' could not be found", dmHashReverseSafe64(bone_name));
        }

        dmScript::PushHash(L, instance_id);
        return 1;
    }

    static int RiveComp_PointerMove(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        RiveComponent* component = 0;
        dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
        lua_Number x = luaL_checknumber(L, 2);
        lua_Number y = luaL_checknumber(L, 3);

        CompRivePointerMove(component, x, y);

        return 0;
    }

    static int RiveComp_PointerUp(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        RiveComponent* component = 0;
        dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
        lua_Number x = luaL_checknumber(L, 2);
        lua_Number y = luaL_checknumber(L, 3);

        CompRivePointerUp(component, x, y);

        return 0;
    }

    static int RiveComp_PointerDown(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        RiveComponent* component = 0;
        dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
        lua_Number x = luaL_checknumber(L, 2);
        lua_Number y = luaL_checknumber(L, 3);

        CompRivePointerDown(component, x, y);

        return 0;
    }


    static int RiveComp_GetTextRun(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        RiveComponent* component = 0;
        dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);

        const char* name = luaL_checkstring(L, 2);
        const char* nested_artboard_path = 0;

        if (lua_isstring(L, 3))
        {
            nested_artboard_path = lua_tostring(L, 3);
        }

        const char* text_run = CompRiveGetTextRun(component, name, nested_artboard_path);

        if (!text_run)
        {
            return DM_LUA_ERROR("The text-run '%s' could not be found.", name);
        }

        lua_pushstring(L, text_run);
        return 1;
    }

    static int RiveComp_SetTextRun(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        RiveComponent* component = 0;
        dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);

        const char* name = luaL_checkstring(L, 2);
        const char* text_run = luaL_checkstring(L, 3);
        const char* nested_artboard_path = 0;

        if (lua_isstring(L, 4))
        {
            nested_artboard_path = lua_tostring(L, 4);
        }

        if (!CompRiveSetTextRun(component, name, text_run, nested_artboard_path))
        {
            return DM_LUA_ERROR("The text-run '%s' could not be found.", name);
        }

        return 0;
    }

    static int RiveComp_GetProjectionMatrix(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmGraphics::HContext graphics_context = dmGraphics::GetInstalledContext();

        float scale_factor = CompRiveGetDisplayScaleFactor();
        float right = (float) dmGraphics::GetWindowWidth(graphics_context) / scale_factor;
        float top = (float) dmGraphics::GetWindowHeight(graphics_context) / scale_factor;

        dmScript::PushMatrix4(L, dmVMath::Matrix4::orthographic(0, right, 0, top, -1, 1));

        return 1;
    }

    static int RiveComp_SetStateMachineInput(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        RiveComponent* component = 0;
        dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);

        const char* input_name = luaL_checkstring(L, 2);
        const char* nested_artboard_path = 0; // optional

        StateMachineInputData data = {};

        if (lua_isnumber(L, 3))
        {
            data.m_Type = StateMachineInputData::TYPE_NUMBER;
            data.m_NumberValue = lua_tonumber(L,3);
        }
        else if (lua_isboolean(L,3))
        {
            data.m_Type = StateMachineInputData::TYPE_BOOL;
            data.m_BoolValue = lua_toboolean(L,3);
        }
        else
        {
            return DM_LUA_ERROR("Cannot set input '%s' with an unsupported type.", input_name);
        }
        
        if (lua_isstring(L, 4))
        {
            nested_artboard_path = lua_tostring(L, 4);
        }

        StateMachineInputData::Result res = CompRiveSetStateMachineInput(component, input_name, nested_artboard_path, data);
        if (res != StateMachineInputData::RESULT_OK)
        {
            if (res == StateMachineInputData::RESULT_TYPE_MISMATCH)
            {
                return DM_LUA_ERROR("Type mismatch for input '%s'.", input_name);
            }
            assert(res == StateMachineInputData::RESULT_NOT_FOUND);
            return DM_LUA_ERROR("The input '%s' could not be found (or an unknown error happened).", input_name);
        }
        return 0;
    }

    static int RiveComp_GetStateMachineInput(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        RiveComponent* component = 0;
        dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);

        const char* input_name = luaL_checkstring(L, 2);
        const char* nested_artboard_path = 0; // optional

        if (lua_isstring(L, 3))
        {
            nested_artboard_path = lua_tostring(L, 3);
        }

        StateMachineInputData data;
        StateMachineInputData::Result res = CompRiveGetStateMachineInput(component, input_name, nested_artboard_path, data);

        if (res != StateMachineInputData::RESULT_OK)
        {
            if (res == StateMachineInputData::RESULT_TYPE_UNSUPPORTED)
            {
                return DM_LUA_ERROR("The input '%s' has an unsupported type.", input_name);
            }
            assert(res == StateMachineInputData::RESULT_NOT_FOUND);
            return DM_LUA_ERROR("The input '%s' could not be found (or an unknown error happened).", input_name);
        }

        switch(data.m_Type)
        {
            case StateMachineInputData::TYPE_BOOL:
                lua_pushboolean(L, data.m_BoolValue);
                break;
            case StateMachineInputData::TYPE_NUMBER:
                lua_pushnumber(L, data.m_NumberValue);
                break;
            case StateMachineInputData::TYPE_INVALID:
                break;
        }
        return 1;
    }

    const char* GetString(lua_State* L, int index, const char* key, uint32_t* out_length)
    {
        const char* result = 0;
        lua_getfield(L, -1, key);
        if (lua_isstring(L, -1))
        {
            size_t len = 0;
            result = lua_tolstring(L, index, &len);
            if (out_length)
                *out_length = (uint32_t)len;
        }
        lua_pop(L, 1);
        return result;
    }

    static int RiveComp_RivSwapAsset(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        dmhash_t rivc_path_hash     = dmScript::CheckHashOrString(L, 1); // path to .rivc
        const char* riv_asset_name  = luaL_checkstring(L, 2); // Name of asset inside the .riv file

        const char* path = 0;
        const char* payload = 0;
        uint32_t payload_size = 0;

        luaL_checktype(L, 3, LUA_TTABLE);
        lua_pushvalue(L, 3);

            path = GetString(L, -1, "path", 0);
            payload = GetString(L, -1, "payload", &payload_size);

        lua_pop(L, 1);

        if (path == 0 && (payload == 0 || payload_size == 0))
        {
            return DM_LUA_ERROR("You must specify either a path or a payload");
        }

        // Temporarily get a reference to the file
        dmRive::RiveSceneData* resource;
        dmResource::Result r = dmResource::Get(g_Factory, rivc_path_hash, (void**)&resource);
        if (dmResource::RESULT_OK != r)
        {
            return DM_LUA_ERROR("Resource was not found: '%s'", dmHashReverseSafe64(rivc_path_hash));
        }

        if (payload)
            r = dmRive::ResRiveDataSetAssetFromMemory(resource, riv_asset_name, (void*)payload, payload_size);
        else
            r = dmRive::ResRiveDataSetAsset(g_Factory, resource, riv_asset_name, path);

        if (dmResource::RESULT_OK != r)
        {
            if (dmResource::RESULT_NOT_SUPPORTED == r)
            {
                return DM_LUA_ERROR("Asset type not supported: '%s'", riv_asset_name);
            }

            if (payload)
            {
                return DM_LUA_ERROR("Failed to load payload for asset: '%s'", riv_asset_name);
            }
            else
            {
                return DM_LUA_ERROR("Failed to load asset: '%s' with path: '%s'", riv_asset_name, path);
            }
        }

        dmResource::Release(g_Factory, resource);
        return 0;
    }

static dmhash_t GetCanonicalPathHash(const char* path)
{
    char canonical_path[1024];
    uint32_t path_len  = dmResource::GetCanonicalPath(path, canonical_path, sizeof(canonical_path));
    return dmHashBuffer64(canonical_path, path_len);
}

// TODO: Move this function into dmScript namespace
static void PreCreateResources(lua_State* L, const char* path_str, const char** supported_exts, uint32_t num_supported_exts, dmhash_t* canonical_path_hash_out)
{
    const char* path_ext = dmResource::GetExtFromPath(path_str);
    while (path_ext[0] == '.')
        path_ext++;

    bool path_ok = false;
    if (path_ext)
    {
        for (uint32_t i = 0; i < num_supported_exts; ++i)
        {
            const char* ext = supported_exts[i];

            if (dmStrCaseCmp(path_ext, ext) == 0)
            {
                path_ok = true;
                break;
            }
        }
    }

    if (!path_ok)
    {
        char message[1024];
        dmSnPrintf(message, sizeof(message), "Unable to create resource, path '%s' must have any of the following extensions: ", path_str);
        for (uint32_t i = 0; i < num_supported_exts; ++i)
        {
            dmStrlCat(message, supported_exts[i], sizeof(message));
        }
        luaL_error(L, "%s", message);
    }

    dmhash_t canonical_path_hash = GetCanonicalPathHash(path_str);

    HResourceDescriptor rd = 0;
    ResourceResult r = ResourceGetDescriptorByHash((HResourceFactory)g_Factory, canonical_path_hash, &rd);
    if (r == RESOURCE_RESULT_OK || rd != 0)
    {
        luaL_error(L, "Unable to create resource, a resource is already registered at path '%s'", path_str);
    }

    *canonical_path_hash_out = canonical_path_hash;
}

static void PreCreateResource(lua_State* L, const char* path_str, const char* path_ext_wanted, dmhash_t* canonical_path_hash_out)
{
    PreCreateResources(L, path_str, &path_ext_wanted, 1, canonical_path_hash_out);
}

static void PushResourceError(lua_State* L, ResourceResult result, dmhash_t path_hash)
{
    const char* format = 0;
    switch(result)
    {
    case RESOURCE_RESULT_RESOURCE_NOT_FOUND: format = "The resource was not found (%d): %llu, %s"; break;
    case RESOURCE_RESULT_NOT_SUPPORTED:      format = "The resource type does not support this operation (%d): %llu, %s"; break;
    default:                                 format = "The resource was not updated (%d): %llu, %s"; break;
    }
    lua_pushfstring(L, format, (unsigned long long)path_hash, dmHashReverseSafe64(path_hash));
}

// TODO: from internal script_resource.cpp. Move to public dmsdk
static int ReportPathError(lua_State* L, ResourceResult result, dmhash_t path_hash)
{
    char msg[256];
    const char* format = 0;
    switch(result)
    {
    case RESOURCE_RESULT_RESOURCE_NOT_FOUND: format = "The resource was not found (%d): %llu, %s"; break;
    case RESOURCE_RESULT_NOT_SUPPORTED:      format = "The resource type does not support this operation (%d): %llu, %s"; break;
    default:                                 format = "The resource was not updated (%d): %llu, %s"; break;
    }
    dmSnPrintf(msg, sizeof(msg), format, result, (unsigned long long)path_hash, dmHashReverseSafe64(path_hash));
    return luaL_error(L, "%s", msg);
}

// TODO: from internal script_resource.cpp. Move to public dmsdk
static void* CheckResource(lua_State* L, dmResource::HFactory factory, dmhash_t path_hash, const char* resource_ext)
{
    HResourceDescriptor rd = 0;
    ResourceResult r = ResourceGetDescriptorByHash((HResourceFactory)factory, path_hash, &rd);
    if (r != RESOURCE_RESULT_OK)
    {
        luaL_error(L, "Could not get %s type resource: %s", resource_ext, dmHashReverseSafe64(path_hash));
        return 0;
    }

    HResourceType expected_resource_type;
    r = ResourceGetTypeFromExtension(factory, resource_ext, &expected_resource_type);
    if( r != RESOURCE_RESULT_OK )
    {
        ReportPathError(L, r, path_hash);
    }

    HResourceType resource_type = ResourceDescriptorGetType(rd);
    if (resource_type != expected_resource_type)
    {
        luaL_error(L, "Resource %s is not of type %s.", dmHashReverseSafe64(path_hash), resource_ext);
        return 0;
    }

    return ResourceDescriptorGetResource(rd);
}

static int RiveComp_CreateRivFromMemory(lua_State* L)
{
    int top = lua_gettop(L);

    const char* path  = luaL_checkstring(L, 1); // path to be associated with this resource

    size_t data_length = 0;
    const char* data = luaL_checklstring(L, 2, &data_length);

    dmGameObject::HInstance instance = dmScript::CheckGOInstance(L);
    dmGameObject::HCollection collection = dmGameObject::GetCollection(instance);

    dmhash_t path_hash;
    PreCreateResource(L, path, "rivc", &path_hash); // create the hash, and check that no other resource is registered with the same hash

    void* resource = 0;
    ResourceResult r = ResourceCreateResource(g_Factory, path, (void*)data, data_length, &resource);
    if (r != RESOURCE_RESULT_OK)
    {
        assert(top == lua_gettop(L));

        lua_pushnil(L);
        PushResourceError(L, r, path_hash);
        return 2;
    }

    // Pass ownership of the resource to the current collection
    dmGameObject::AddDynamicResourceHash(collection, path_hash);
    dmScript::PushHash(L, path_hash);
    lua_pushnil(L);

    assert((top+2) == lua_gettop(L));
    return 2;
}


    // This is an "all bets are off" mode.
    static int RiveComp_DebugSetBlitMode(lua_State* L)
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

    static const luaL_reg RIVE_FUNCTIONS[] =
    {
        {"play_anim",               RiveComp_PlayAnim},
        {"play_state_machine",      RiveComp_PlayStateMachine},
        {"cancel",                  RiveComp_Cancel},
        {"get_go",                  RiveComp_GetGO},
        {"pointer_move",            RiveComp_PointerMove},
        {"pointer_up",              RiveComp_PointerUp},
        {"pointer_down",            RiveComp_PointerDown},
        {"set_text_run",            RiveComp_SetTextRun},
        {"get_text_run",            RiveComp_GetTextRun},
        {"get_projection_matrix",   RiveComp_GetProjectionMatrix},
        {"get_state_machine_input", RiveComp_GetStateMachineInput},
        {"set_state_machine_input", RiveComp_SetStateMachineInput},
        {"debug_set_blit_mode",     RiveComp_DebugSetBlitMode},

        {"riv_swap_asset",          RiveComp_RivSwapAsset},

        {"create_riv_from_memory",  RiveComp_CreateRivFromMemory},
        {0, 0}
    };

    void ScriptRegister(lua_State* L, dmResource::HFactory factory)
    {
        // deprecated
        luaL_register(L, "rive", RIVE_FUNCTIONS);
            ScriptInitializeDataBinding(L, factory);
        lua_pop(L, 1);

        ScriptInitializeFile(L, factory);
        ScriptInitializeArtboard(L, factory);
        ScriptInitializeViewModel(L, factory);
        ScriptInitializeViewModelProperty(L, factory);

        g_Factory = factory;
    }
}

#endif // DM_RIVE_UNSUPPORTED
