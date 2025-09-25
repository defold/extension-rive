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

#define SCRIPT_LIB_NAME "rive"
#define SCRIPT_TYPE_NAME_ARTBOARD           "Artboard"
#define SCRIPT_TYPE_NAME_VIEWMODELINSTANCE  "ViewModelInstance"

enum ScriptStructType
{
    SCRIPT_TYPE_ARTBOARD,
    SCRIPT_TYPE_VIEWMODELINSTANCE,
    SCRIPT_TYPE_MAX,
};

static uint32_t TYPE_HASHES[SCRIPT_TYPE_MAX];
static dmResource::HFactory g_Factory = 0;

struct Artboard;
struct ViewModelInstanceRuntime;


static dmhash_t g_ResourceTypeNameHashRivc = dmHashString64("rivc");

static bool IsTypeRivc(HResourceFactory factory, dmhash_t path_hash)
{
    HResourceDescriptor rd = 0;
    ResourceResult result = ResourceGetDescriptorByHash(factory, path_hash, &rd);
    if (RESOURCE_RESULT_OK != result)
    {
        return false;
    }
    HResourceType type = ResourceDescriptorGetType(rd);
    dmhash_t type_hash = ResourceTypeGetNameHash(type);
    return type_hash == g_ResourceTypeNameHashRivc;
}



// *********************************************************************************

struct Artboard
{
    RiveSceneData* m_Resource;                          // The original file that it belongs to
    std::unique_ptr<rive::ArtboardInstance> m_Artboard; // The actual artboard instance
};

static bool IsArtboard(lua_State* L, int index)
{
    return dmScript::GetUserType(L, index) == TYPE_HASHES[SCRIPT_TYPE_ARTBOARD];
}

static Artboard* ToArtboard(lua_State* L, int index)
{
    return (Artboard*)dmScript::ToUserType(L, index, TYPE_HASHES[SCRIPT_TYPE_ARTBOARD]);
}

static int Artboard_tostring(lua_State* L)
{
    Artboard* artboard = ToArtboard(L, 1);
    if (!artboard)
    {
        lua_pushstring(L, "no valid pointer!");
        return 1;
    }
    lua_pushfstring(L, "%s.artboard(%p : %s)", SCRIPT_LIB_NAME, artboard, artboard->m_Artboard ? artboard->m_Artboard->name().c_str() : "");
    return 1;
}

static int Artboard_index(lua_State* L)
{
    Artboard* artboard = ToArtboard(L, 1);
    const char* key = luaL_checkstring(L, 2);

    // // check the inputs.
    // // They don't all share the same api, and they don't have a data type enum to compare
    // rive::ArtboardInstance* ab = artboard->m_Artboard.get();
    // rive::SMIBool* rbool = ab->getBool()
    // if ()

    // SMIBool* getBool(const std::string& name, const std::string& path);
    // SMINumber* getNumber(const std::string& name, const std::string& path);
    // SMITrigger* getTrigger(const std::string& name, const std::string& path);
    // TextValueRun* getTextRun(const std::string& name, const std::string& path);

    // if (key[0] == 'x')
    // {
    //     lua_pushnumber(L, v->getX());
    //     return 1;
    // }
    // else if (key[0] == 'y')
    // {
    //     lua_pushnumber(L, v->getY());
    //     return 1;
    // }
    // else if (key[0] == 'z')
    // {
    //     lua_pushnumber(L, v->getZ());
    //     return 1;
    // }
    // else
    {
        return luaL_error(L, "%s.%s currently have no fields.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_ARTBOARD);
    }
}

static int Artboard_newindex(lua_State* L)
{
    Artboard* v = (Artboard*)lua_touserdata(L, 1);

    // const char* key = luaL_checkstring(L, 2);
    // if (key[0] == 'x')
    // {
    //     v->setX((float) luaL_checknumber(L, 3));
    // }
    // else if (key[0] == 'y')
    // {
    //     v->setY((float) luaL_checknumber(L, 3));
    // }
    // else if (key[0] == 'z')
    // {
    //     v->setZ((float) luaL_checknumber(L, 3));
    // }
    // else
    {
        return luaL_error(L, "%s.%s only has fields x, y, z.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_ARTBOARD);
    }
    return 0;
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
    artboard->m_Artboard.reset();
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
    {"__index",     Artboard_index},
    {"__newindex",  Artboard_newindex},
    {"__eq",        Artboard_eq},
    {"__gc",        Artboard_gc},
    {0,0}
};

static void PushArtboard(lua_State* L, RiveSceneData* resource, std::unique_ptr<rive::ArtboardInstance>& instance)
{
    Artboard* artboard = (Artboard*)lua_newuserdata(L, sizeof(Artboard));
    artboard->m_Resource = resource;
    artboard->m_Artboard = std::move(instance);
    luaL_getmetatable(L, SCRIPT_TYPE_NAME_ARTBOARD);
    lua_setmetatable(L, -2);
}

static Artboard* CheckArtboard(lua_State* L, int index)
{
    Artboard* artboard = (Artboard*)dmScript::CheckUserType(L, index, TYPE_HASHES[SCRIPT_TYPE_ARTBOARD], 0);
    return artboard;
}

static RiveSceneData* AcquireRiveResource(lua_State* L, int index)
{
    if (dmScript::IsURL(L, index))
    {
        RiveComponent* component = 0;
        dmScript::GetComponentFromLua(L, index, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
        RiveSceneData* resource = CompRiveGetRiveSceneData(component);
        if (resource)
        {
            dmResource::IncRef(g_Factory, resource);
        }
        return resource;
    }

    dmhash_t path_hash = dmScript::CheckHashOrString(L, index); // the resource to get the artboard from

    if (!IsTypeRivc(g_Factory, path_hash))
    {
        luaL_error(L, "Requested resource was not of type 'rivc': %s", dmHashReverseSafe64(path_hash));
        return 0;
    }

    dmRive::RiveSceneData* resource;
    dmResource::Result r = dmResource::Get(g_Factory, path_hash, (void**)&resource);
    if (dmResource::RESULT_OK != r)
    {
        luaL_error(L, "Resource was not found: '%s'", dmHashReverseSafe64(path_hash));
        return 0;
    }
    return resource;
}

static int ArtboardGet(lua_State* L)
{
    int top = lua_gettop(L);

    RiveSceneData* resource = AcquireRiveResource(L, 1);
    if (!resource)
    {
        return luaL_error(L, "Failed to get rive resource");
    }

    const char* name = 0;
    if (!lua_isnil(L, 2))
        name = luaL_checkstring(L, 2); // the name of the artboard (todo: could support indices later on)

    std::unique_ptr<rive::ArtboardInstance> instance;
    if (name == 0)
        instance = resource->m_File->artboardDefault();
    else
        instance = resource->m_File->artboardNamed(name);

    PushArtboard(L, resource, instance);

    assert((top+1) == lua_gettop(L));
    return 1;
}

// *********************************************************************************

// struct ViewModelInstance
// {
//     RiveSceneData* m_Resource;                          // The original file that it belongs to
//     std::unique_ptr<rive::ArtboardInstance> m_Artboard; // The actual artboard instance
// };

// static bool IsArtboard(lua_State* L, int index)
// {
//     return dmScript::GetUserType(L, index) == TYPE_HASHES[SCRIPT_TYPE_ARTBOARD];
// }

// static Artboard* ToArtboard(lua_State* L, int index)
// {
//     return (Artboard*)dmScript::ToUserType(L, index, TYPE_HASHES[SCRIPT_TYPE_ARTBOARD]);
// }

// static int Artboard_tostring(lua_State* L)
// {
//     Artboard* artboard = ToArtboard(L, 1);
//     if (!artboard)
//     {
//         lua_pushstring(L, "no valid pointer!");
//         return 1;
//     }
//     lua_pushfstring(L, "%s.artboard(%p : %s)", SCRIPT_LIB_NAME, artboard, artboard->m_Artboard ? artboard->m_Artboard->name().c_str() : "");
//     return 1;
// }

// static int Artboard_index(lua_State* L)
// {
//     Artboard* artboard = ToArtboard(L, 1);
//     const char* key = luaL_checkstring(L, 2);

//     // // check the inputs.
//     // // They don't all share the same api, and they don't have a data type enum to compare
//     // rive::ArtboardInstance* ab = artboard->m_Artboard.get();
//     // rive::SMIBool* rbool = ab->getBool()
//     // if ()

//     // SMIBool* getBool(const std::string& name, const std::string& path);
//     // SMINumber* getNumber(const std::string& name, const std::string& path);
//     // SMITrigger* getTrigger(const std::string& name, const std::string& path);
//     // TextValueRun* getTextRun(const std::string& name, const std::string& path);

//     // if (key[0] == 'x')
//     // {
//     //     lua_pushnumber(L, v->getX());
//     //     return 1;
//     // }
//     // else if (key[0] == 'y')
//     // {
//     //     lua_pushnumber(L, v->getY());
//     //     return 1;
//     // }
//     // else if (key[0] == 'z')
//     // {
//     //     lua_pushnumber(L, v->getZ());
//     //     return 1;
//     // }
//     // else
//     {
//         return luaL_error(L, "%s.%s currently have no fields.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_ARTBOARD);
//     }
// }

// static int Artboard_newindex(lua_State* L)
// {
//     Artboard* v = (Artboard*)lua_touserdata(L, 1);

//     // const char* key = luaL_checkstring(L, 2);
//     // if (key[0] == 'x')
//     // {
//     //     v->setX((float) luaL_checknumber(L, 3));
//     // }
//     // else if (key[0] == 'y')
//     // {
//     //     v->setY((float) luaL_checknumber(L, 3));
//     // }
//     // else if (key[0] == 'z')
//     // {
//     //     v->setZ((float) luaL_checknumber(L, 3));
//     // }
//     // else
//     {
//         return luaL_error(L, "%s.%s only has fields x, y, z.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_ARTBOARD);
//     }
//     return 0;
// }

// static int Artboard_eq(lua_State* L)
// {
//     Artboard* a1 = ToArtboard(L, 1);
//     Artboard* a2 = ToArtboard(L, 2);
//     lua_pushboolean(L, a1 && a2 && a1 == a2);
//     return 1;
// }

// static int Artboard_gc(lua_State* L)
// {
//     Artboard* artboard = ToArtboard(L, 1);
//     artboard->m_Artboard.reset();
//     dmResource::Release(g_Factory, artboard->m_Resource);
//     return 0;
// }

// static const luaL_reg Artboard_methods[] =
// {
//     {0,0}
// };

// static const luaL_reg Artboard_meta[] =
// {
//     {"__tostring",  Artboard_tostring},
//     {"__index",     Artboard_index},
//     {"__newindex",  Artboard_newindex},
//     {"__eq",        Artboard_eq},
//     {"__gc",        Artboard_gc},
//     {0,0}
// };

// static void PushArtboard(lua_State* L, RiveSceneData* resource, std::unique_ptr<rive::ArtboardInstance>& instance)
// {
//     Artboard* artboard = (Artboard*)lua_newuserdata(L, sizeof(Artboard));
//     artboard->m_Resource = resource;
//     artboard->m_Artboard = std::move(instance);
//     luaL_getmetatable(L, SCRIPT_TYPE_NAME_ARTBOARD);
//     lua_setmetatable(L, -2);
// }

// static Artboard* CheckArtboard(lua_State* L, int index)
// {
//     Artboard* artboard = (Artboard*)dmScript::CheckUserType(L, index, TYPE_HASHES[SCRIPT_TYPE_ARTBOARD], 0);
//     return artboard;
// }

// static int ViewModelInstanceGet(lua_State* L)
// {
//     DM_LUA_STACK_CHECK(L, 1);
//     RiveComponent* component = 0;
//     dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
//     uint32_t handle = CompRiveGetViewModelInstanceRuntime(component);
//     lua_pushinteger(L, handle);
//     return 1;
// }



// *********************************************************************************

static const luaL_reg STRUCT_FUNCTIONS[] =
{
    {"get_artboard",          ArtboardGet},
    //{"get_viewmodelinstance", ViewModelInstanceGet},
    {0, 0}
};

void ScriptInitializeStructs(lua_State* L, dmResource::HFactory factory)
{
    int top = lua_gettop(L);

    g_Factory = factory;

    struct
    {
        const char*     m_Name;
        const luaL_reg* m_Methods;
        const luaL_reg* m_Metatable;
        uint32_t*       m_TypeHash;
    } types[] =
    {
        {"Artboard", Artboard_methods, Artboard_meta, &TYPE_HASHES[SCRIPT_TYPE_ARTBOARD]},
    };

    for (uint32_t i = 0; i < DM_ARRAY_SIZE(types); ++i)
    {
        *types[i].m_TypeHash = dmScript::RegisterUserType(L, types[i].m_Name, types[i].m_Methods, types[i].m_Metatable);
    }

    luaL_register(L, SCRIPT_LIB_NAME, STRUCT_FUNCTIONS);
    lua_pop(L, 1);

    assert(top == lua_gettop(L));
}

}

#endif // DM_RIVE_UNSUPPORTED
