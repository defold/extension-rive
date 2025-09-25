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

static dmResource::HFactory g_Factory = 0;

static dmhash_t     RESOURCE_TYPE_HASH_RIVC = dmHashString64("rivc");
static const char*  SCRIPT_TYPE_NAME_RIVE_FILE = "RiveFile";
static uint32_t     TYPE_HASH_RIVE_FILE = 0;

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
    return type_hash == RESOURCE_TYPE_HASH_RIVC;
}



// *********************************************************************************

struct RiveFile
{
    RiveSceneData* m_Resource;
};

static bool IsRiveFile(lua_State* L, int index)
{
    return dmScript::GetUserType(L, index) == TYPE_HASH_RIVE_FILE;
}

static RiveFile* ToRiveFile(lua_State* L, int index)
{
    return (RiveFile*)dmScript::ToUserType(L, index, TYPE_HASH_RIVE_FILE);
}

static int RiveFile_tostring(lua_State* L)
{
    RiveFile* file = ToRiveFile(L, 1);
    if (!file)
    {
        lua_pushstring(L, "no valid pointer!");
        return 1;
    }
    lua_pushfstring(L, "%s.file(%p : %s)", "rive", file, dmHashReverseSafe64(file->m_Resource->m_PathHash));
    return 1;
}

static int RiveFile_eq(lua_State* L)
{
    RiveFile* f1 = ToRiveFile(L, 1);
    RiveFile* f2 = ToRiveFile(L, 2);
    lua_pushboolean(L, f1 && f2 && f1 == f2);
    return 1;
}

static int RiveFile_gc(lua_State* L)
{
    RiveFile* file = ToRiveFile(L, 1);
    dmResource::Release(g_Factory, file->m_Resource);
    return 0;
}

static const luaL_reg RiveFile_methods[] =
{
    {0,0}
};

static const luaL_reg RiveFile_meta[] =
{
    {"__tostring",  RiveFile_tostring},
    // {"__index",     RiveFile_index},
    // {"__newindex",  RiveFile_newindex},
    {"__eq",        RiveFile_eq},
    {"__gc",        RiveFile_gc},
    {0,0}
};

static void PushRiveFile(lua_State* L, RiveSceneData* resource)
{
    RiveFile* file = (RiveFile*)lua_newuserdata(L, sizeof(RiveFile));
    file->m_Resource = resource;
    luaL_getmetatable(L, SCRIPT_TYPE_NAME_RIVE_FILE);
    lua_setmetatable(L, -2);
}

static RiveFile* CheckArtboard(lua_State* L, int index)
{
    RiveFile* file = (RiveFile*)dmScript::CheckUserType(L, index, TYPE_HASH_RIVE_FILE, 0);
    return file;
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

static int FileGet(lua_State* L)
{
    int top = lua_gettop(L);

    RiveSceneData* resource = AcquireRiveResource(L, 1);
    if (!resource)
    {
        return luaL_error(L, "Failed to get rive resource");
    }

    PushRiveFile(L, resource);

    assert((top+1) == lua_gettop(L));
    return 1;
}


// *********************************************************************************

static const luaL_reg RIVE_FILE_FUNCTIONS[] =
{
    {"get_file", FileGet},
    {0, 0}
};

void ScriptInitializeFile(lua_State* L, dmResource::HFactory factory)
{
    int top = lua_gettop(L);

    g_Factory = factory;

    struct
    {
        const char*     m_Name;
        const luaL_reg* m_Methods;
        const luaL_reg* m_Metatable;
        uint32_t*       m_TypeHash;
    } types[1] =
    {
        {SCRIPT_TYPE_NAME_RIVE_FILE, RiveFile_methods, RiveFile_meta, &TYPE_HASH_RIVE_FILE},
    };

    for (uint32_t i = 0; i < DM_ARRAY_SIZE(types); ++i)
    {
        *types[i].m_TypeHash = dmScript::RegisterUserType(L, types[i].m_Name, types[i].m_Methods, types[i].m_Metatable);
    }

    luaL_register(L, "rive", RIVE_FILE_FUNCTIONS);
    lua_pop(L, 1);

    assert(top == lua_gettop(L));
}

}

#endif // DM_RIVE_UNSUPPORTED
