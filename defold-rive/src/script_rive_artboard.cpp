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
#include <dmsdk/script/script.h>
#include <dmsdk/resource/resource.h>

#include "comp_rive.h"
#include "comp_rive_private.h"
#include "res_rive_data.h"
#include "script_rive_private.h"
#include <rive/data_bind/data_values/data_type.hpp>

#include "script_defold.h"
#include <rive/lua/rive_lua_libs.hpp>
#include <rive_lua_wrapper.h>

namespace dmRive
{

static dmResource::HFactory g_Factory = 0;

static const char*  SCRIPT_TYPE_NAME_ARTBOARD = rive::ScriptedArtboard::luaName;
static char SCRIPT_TYPE_ARTBOARD_METATABLE_NAME[128];

// *********************************************************************************

static rive::ScriptedArtboard* ToArtboard(lua_State* L, int index)
{
    return (rive::ScriptedArtboard*)dmRive::ToUserType(L, index, SCRIPT_TYPE_ARTBOARD_METATABLE_NAME);
}

static int Artboard_tostring(lua_State* L)
{
    rive::ScriptedArtboard* artboard = ToArtboard(L, 1);
    if (!artboard)
    {
        lua_pushstring(L, "no valid pointer!");
        return 1;
    }
    lua_pushfstring(L, "rive.%s(%p : '%s')", rive::ScriptedArtboard::luaName, artboard, artboard->artboard()->name().c_str());
    return 1;
}

static int Artboard_gc(lua_State* L)
{
    rive::ScriptedArtboard* artboard = ToArtboard(L, 1);
    artboard->~ScriptedArtboard();
    return 0;
}

static int Artboard_advance(lua_State* L)
{
    rive::ScriptedArtboard* artboard = ToArtboard(L, 1);
    lua_Number seconds = luaL_checknumber(L, 2);
    artboard->advance(seconds);
    return 0;
}

static int Artboard_index(lua_State* L)
{
    rive::ScriptedArtboard* artboard = ToArtboard(L, 1);
    if (lua_type(L, 2) == LUA_TSTRING)
    {
        const char* key = lua_tostring(L, 2);
        //const char* key = luaL_checkstring(L, 2);
        if (strcmp("frameOrigin", key) == 0)
        {
            lua_pushboolean(L, artboard->artboard()->frameOrigin());
            return 1;
        }
        else if (strcmp("data", key) == 0)
        {
            return artboard->pushData(L);
        }
    }

    // Fallback to methods
    return PushFromMetaMethods(L, 1, 2);
}

static int Artboard_newindex(lua_State* L)
{
    rive::ScriptedArtboard* artboard = ToArtboard(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (strcmp("frameOrigin", key) == 0)
    {
        bool v = lua_toboolean(L, 3);
        artboard->artboard()->frameOrigin(v);
        return 1;
    }

    return luaL_error(L, "rive.%s: unknown property '%s'", rive::ScriptedArtboard::luaName, key);
}

static const luaL_reg Artboard_methods[] =
{
    {"advance", Artboard_advance},
    {0,0}
};

static const luaL_reg Artboard_meta[] =
{
    {"__tostring",  Artboard_tostring},
    {"__gc",        Artboard_gc},
    {"__index",     Artboard_index},
    {"__newindex",  Artboard_newindex},
    {0,0}
};

void ScriptInitializeArtboard(lua_State* L, dmResource::HFactory factory)
{
    int top = lua_gettop(L);

    g_Factory = factory;

    GetMetaTableName(rive::ScriptedArtboard::luaName, SCRIPT_TYPE_ARTBOARD_METATABLE_NAME, sizeof(SCRIPT_TYPE_ARTBOARD_METATABLE_NAME));
    dmRive::RegisterUserType(L, SCRIPT_TYPE_NAME_ARTBOARD, SCRIPT_TYPE_ARTBOARD_METATABLE_NAME, Artboard_methods, Artboard_meta);

    assert(top == lua_gettop(L));
}

}

#endif // DM_RIVE_UNSUPPORTED
