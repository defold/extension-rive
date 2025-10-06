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

#ifndef DM_RIVE_LUA_WRAPPER_H
#define DM_RIVE_LUA_WRAPPER_H

extern "C" {
    #include <dmsdk/lua/lua.h>
    #include <dmsdk/lua/lauxlib.h>
}
#include <dmsdk/dlib/dstrings.h>

// Defold helpers to aid with using classes directly from the rive-runtime
#include <assert.h>
#include <utility>
static const uint32_t LUA_T_COUNT = 80; // after defold types

namespace dmRive
{
    void* ToUserType(lua_State* L, int index);
}

inline
const char* GetMetaTableName(const char* name, char* buffer, uint32_t buffer_size)
{
    dmSnPrintf(buffer, buffer_size, "%s_mt", name);
    return buffer;
}

template <class T, class... Args>
static T* lua_newrive(lua_State* L, Args&&... args)
{
    char mtname[256];
    GetMetaTableName(T::luaName, mtname, sizeof(mtname));
    printf("MAWE lua_newrive: %s / %s\n", T::luaName, mtname);

    T* instance = (T*)lua_newuserdata(L, sizeof(T));
    luaL_getmetatable(L, mtname);
    lua_setmetatable(L, -2);
    return new (instance) T(std::forward<Args>(args)...);
}

template <typename T>
static T* lua_torive(lua_State* L, int idx, bool allowNil = false)
{
    //T* riveObject = (T*)lua_touserdatatagged(L, idx, T::luaTag);
    T* riveObject = (T*)dmRive::ToUserType(L, idx);

    if (!allowNil && riveObject == nullptr)
    {
        luaL_typeerror(L, idx, T::luaName);
        return nullptr;
    }
    return riveObject;
}

inline
void lua_pushunsigned(lua_State* L, uint32_t value)
{
    lua_pushinteger(L, value);
}

inline
void luaL_typeerrorL(lua_State* L, int index, const char* type)
{
    luaL_error(L, "argument at index %d was not of type %s", index, type);
}


// end defold

#endif // DM_RIVE_LUA_WRAPPER_H
