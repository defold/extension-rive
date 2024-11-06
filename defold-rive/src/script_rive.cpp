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
#include <dmsdk/gameobject/script.h>
#include <dmsdk/gamesys/script.h>

#include "comp_rive.h"
#include "rive_ddf.h"

namespace dmRive
{
    static const char*    RIVE_EXT      = "rivc";
    static const dmhash_t RIVE_EXT_HASH = dmHashString64(RIVE_EXT);

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
            return DM_LUA_ERROR("The text-run '%s' could not be found, or the text is empty.", name);
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
        const char* nested_artboard_path = 0;

        if (lua_isstring(L, 4))
        {
            nested_artboard_path = lua_tostring(L, 4);
        }

        const char* text_run = luaL_checkstring(L, 3);

        if (!CompRiveSetTextRun(component, name, text_run, nested_artboard_path))
        {
            return DM_LUA_ERROR("The text-run '%s' could not be found, or the text is empty.", name);
        }

        return 0;
    }

    static const luaL_reg RIVE_FUNCTIONS[] =
    {
        {"play_anim",           RiveComp_PlayAnim},
        {"play_state_machine",  RiveComp_PlayStateMachine},
        {"cancel",              RiveComp_Cancel},
        {"get_go",              RiveComp_GetGO},
        {"pointer_move",        RiveComp_PointerMove},
        {"pointer_up",          RiveComp_PointerUp},
        {"pointer_down",        RiveComp_PointerDown},
        {"set_text_run",        RiveComp_SetTextRun},
        {"get_text_run",        RiveComp_GetTextRun},
        {0, 0}
    };

    void ScriptRegister(lua_State* L)
    {
        luaL_register(L, "rive", RIVE_FUNCTIONS);
        lua_pop(L, 1);
    }
}

#endif // DM_RIVE_UNSUPPORTED
