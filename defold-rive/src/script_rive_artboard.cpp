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

#include <rive/file.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include <rive/animation/loop.hpp>
#include <rive/animation/state_machine.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/custom_property.hpp>
#include <rive/custom_property_boolean.hpp>
#include <rive/custom_property_number.hpp>
#include <rive/custom_property_string.hpp>
#include <rive/math/mat2d.hpp>
#include <rive/nested_artboard.hpp> // Artboard
#include <rive/renderer.hpp>
#include <rive/text/text.hpp>


namespace dmRive
{

static dmResource::HFactory g_Factory = 0;

static const char*  SCRIPT_TYPE_NAME_ARTBOARD = "Artboard";
static uint32_t     TYPE_HASH_ARTBOARD = 0;


// *********************************************************************************

static bool IsArtboard(lua_State* L, int index)
{
    return dmScript::GetUserType(L, index) == TYPE_HASH_ARTBOARD;
}

static Artboard* ToArtboard(lua_State* L, int index)
{
    return (Artboard*)dmScript::ToUserType(L, index, TYPE_HASH_ARTBOARD);
}

static int Artboard_tostring(lua_State* L)
{
    Artboard* artboard = ToArtboard(L, 1);
    if (!artboard)
    {
        lua_pushstring(L, "no valid pointer!");
        return 1;
    }
    lua_pushfstring(L, "rive.artboard(%p : '%s')", artboard, artboard->m_Instance ? artboard->m_Instance->name().c_str() : "null");
    return 1;
}

static int Artboard_eq(lua_State* L)
{
    Artboard* a1 = ToArtboard(L, 1);
    Artboard* a2 = ToArtboard(L, 2);
    lua_pushboolean(L, a1 && a2 && a1 == a2);
    return 1;
}

static int Artboard_gc(lua_State* L)
{
    Artboard* artboard = ToArtboard(L, 1);
    delete artboard->m_Instance;
    dmResource::Release(g_Factory, artboard->m_Resource);
    return 0;
}

static const luaL_reg Artboard_methods[] =
{
    {0,0}
};

static const luaL_reg Artboard_meta[] =
{
    {"__tostring",  Artboard_tostring},
    {"__eq",        Artboard_eq},
    {"__gc",        Artboard_gc},
    {0,0}
};

void PushArtboard(lua_State* L, RiveSceneData* resource, rive::ArtboardInstance* instance)
{
    Artboard* artboard = (Artboard*)lua_newuserdata(L, sizeof(Artboard));
    artboard->m_Resource = resource;
    artboard->m_Instance = instance;
    luaL_getmetatable(L, SCRIPT_TYPE_NAME_ARTBOARD);
    lua_setmetatable(L, -2);
    dmResource::IncRef(g_Factory, resource);
}

Artboard* CheckArtboard(lua_State* L, int index)
{
    Artboard* artboard = (Artboard*)dmScript::CheckUserType(L, index, TYPE_HASH_ARTBOARD, 0);
    return artboard;
}

void ScriptInitializeArtboard(lua_State* L, dmResource::HFactory factory)
{
    int top = lua_gettop(L);

    g_Factory = factory;

    TYPE_HASH_ARTBOARD = dmScript::RegisterUserType(L, SCRIPT_TYPE_NAME_ARTBOARD, Artboard_methods, Artboard_meta);

    assert(top == lua_gettop(L));
}

}

#endif // DM_RIVE_UNSUPPORTED
