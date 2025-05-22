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

#include <stdint.h>

#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/gameobject/script.h>
#include <dmsdk/gamesys/script.h>

#include "comp_rive.h"
#include "comp_rive_private.h"

namespace dmRive
{

static int CreateViewModelInstanceRuntime(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 2);

    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);

    dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);

    uint32_t handle = dmRive::CompRiveCreateViewModelInstanceRuntime(component, name_hash);
    if (handle)
    {
        lua_pushinteger(L, handle);
        lua_pushnil(L);
    }
    else
    {
        lua_pushnil(L);
        char msg[256];
        dmSnPrintf(msg, sizeof(msg), "Failed to create view model instance runtime with name %s", dmHashReverseSafe64(name_hash));
        lua_pushstring(L, msg);
    }
    return 2;
}

static int DestroyViewModelInstanceRuntime(lua_State* L)
{
    return 1;
}

static int SetViewModelInstanceRuntime(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);

    uint32_t handle = luaL_checkinteger(L, 2);
    bool result = dmRive::CompRiveSetViewModelInstanceRuntime(component, handle);
    if (!result)
    {
        return DM_LUA_ERROR("Could not set view model instance runtime with handle '%u'", handle);
    }

    return 0;
}

static int GetViewModelInstanceRuntime(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
    uint32_t handle = CompRiveGetViewModelInstanceRuntime(component);
    lua_pushinteger(L, handle);
    return 1;
}

static int SetProperties(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);

    uint32_t handle = luaL_checkinteger(L, 2);

    printf("SETPROPERTIES:\n");

    luaL_checktype(L, 3, LUA_TTABLE);
    lua_pushvalue(L, 3);
    lua_pushnil(L);
    while (lua_next(L, -2)) {
        // Note: This is a "path", as it may refer to a property within a nested structure of view model instance runtimes
        const char* path = lua_tostring(L, -2);
        printf("PATH: '%s'\n", path);

        dmVMath::Vector4* color = 0;
        if ((color = dmScript::ToVector4(L, -1)))
        {
            printf("  color: %.2f %.2f %.2f %.2f\n", color->getX(), color->getY(), color->getZ(), color->getW());
            dmRive::CompRiveRuntimePropertyColor(component, handle, path, color);
        }
        else if (lua_isnumber(L, -1))
        {
            float number = lua_tonumber(L, -1);
            printf("  number: %f\n", number);
            dmRive::CompRiveRuntimePropertyF32(component, handle, path, number);
        }
        else if (lua_isboolean(L, -1))
        {
            bool boolean = lua_toboolean(L, -1);
            printf("  boolean: %d\n", (int)boolean);
        }
        else if (lua_isstring(L, -1))
        {
            const char* str = lua_tostring(L, -1);
            printf("  string: '%s'\n", str?str:"");
        }
        else
        {
            // In Lua, we cannot easily represent Rive types: enum, list and trigger
            // See data_type.hpp

        }

        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    return 0;
}

static int SetListener(lua_State* L)
{
    return 0;
}

static const luaL_reg RIVE_DATABIND_FUNCTIONS[] =
{
    {"create_view_model_instance_runtime",  CreateViewModelInstanceRuntime},
    {"destroy_view_model_instance_runtime", DestroyViewModelInstanceRuntime},
    {"set_view_model_instance_runtime",     SetViewModelInstanceRuntime},
    {"get_view_model_instance_runtime",     GetViewModelInstanceRuntime},
    {"set_properties",                      SetProperties},
    {"set_listener",                        SetListener},
    {0, 0}
};

void ScriptInitializeDataBinding(struct lua_State* L)
{
    lua_newtable(L);
    luaL_register(L, 0, RIVE_DATABIND_FUNCTIONS);
    lua_setfield(L, -2, "databind");
}

} // namespace dmRive

#endif // DM_RIVE_UNSUPPORTED
