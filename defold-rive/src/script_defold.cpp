// Copyright 2020 The Defold Foundation
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


#include <dmsdk/script/script.h>

namespace dmRive
{

int PushFromMetaMethods(lua_State* L, int obj_index, int key_index)
{
    if (lua_getmetatable(L, obj_index))
    {
        lua_getfield(L, -1, "__methods");
        lua_pushvalue(L, key_index);
        lua_rawget(L, -2);
        lua_remove(L, -2); // pop methods table
        lua_remove(L, -2); // pop metatable
        return 1;
    }
    // No metatable or no __methods — push nil
    lua_pushnil(L);
    return 1;
}


// Helper: set functions from a luaL_reg array onto table at top of stack
static void SetFuncs(lua_State* L, const luaL_reg* funcs)
{
    if (!funcs) return;
    for (const luaL_reg* r = funcs; r->name; ++r)
    {
        lua_pushcfunction(L, r->func);
        lua_setfield(L, -2, r->name);
    }
}

void RegisterUserType(lua_State* L, const char* class_name, const char* metatable_name, const luaL_reg methods[], const luaL_reg meta[])
{
    // Methods table
    lua_newtable(L);
    int methods_idx = lua_gettop(L);
    SetFuncs(L, methods);

    // Metatable
    luaL_newmetatable(L, metatable_name);
    int mt = lua_gettop(L);
    // Store methods table on metatable for generic __index helpers
    lua_pushvalue(L, methods_idx);
    lua_setfield(L, mt, "__methods");

    // Apply/override metamethods from provided list
    SetFuncs(L, meta);

    // pop metatable
    lua_pop(L, 1);

    // Pop methods table
    lua_pop(L, 1);
}

static void* luaL_testudata(lua_State* L, int idx, const char* tname)
{
    void* p = lua_touserdata(L, idx);
    if (p == NULL)
    {
        return NULL;
    }
    if (!lua_getmetatable(L, idx))
    {
        return NULL;
    }
    luaL_getmetatable(L, tname);
    int equal = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);
    if (!equal)
    {
        return NULL;
    }
    return p;
}

void* ToUserType(lua_State* L, int idx, const char* metatable_name)
{
    return luaL_testudata(L, idx, metatable_name);
}

void* CheckUserType(lua_State* L, int idx, const char* metatable_name)
{
    return luaL_checkudata(L, idx, metatable_name);
}

}
