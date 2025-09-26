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

// #include <dmsdk/gameobject/script.h>
// #include <dmsdk/gamesys/script.h>
#include <dmsdk/script/script.h>
#include <dmsdk/resource/resource.h>

#include "comp_rive.h"
#include "comp_rive_private.h"
#include "res_rive_data.h"
#include "script_rive_private.h"

#include <rive/viewmodel/runtime/viewmodel_instance_runtime.hpp>
#include <rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp>
#include <rive/viewmodel/runtime/viewmodel_instance_number_runtime.hpp>

namespace dmRive
{

static dmResource::HFactory g_Factory = 0;

static const char*  SCRIPT_TYPE_NAME_VIEWMODEL = "ViewModel";
static uint32_t     TYPE_HASH_VIEWMODEL = 0;

// *********************************************************************************

static bool IsViewModel(lua_State* L, int index)
{
    return dmScript::GetUserType(L, index) == TYPE_HASH_VIEWMODEL;
}

static ViewModel* ToViewModel(lua_State* L, int index)
{
    return (ViewModel*)dmScript::ToUserType(L, index, TYPE_HASH_VIEWMODEL);
}

static int ViewModel_tostring(lua_State* L)
{
    ViewModel* viewmodel = ToViewModel(L, 1);
    if (!viewmodel)
    {
        lua_pushstring(L, "no valid pointer!");
        return 1;
    }
    lua_pushfstring(L, "rive.viewmodel(%p : '%s')", viewmodel, viewmodel->m_Instance ? viewmodel->m_Instance->name().c_str() : "null");
    return 1;
}

static int ViewModel_eq(lua_State* L)
{
    ViewModel* v1 = ToViewModel(L, 1);
    ViewModel* v2 = ToViewModel(L, 2);
    lua_pushboolean(L, v1 && v2 && v1 == v2);
    return 1;
}

static int ViewModel_gc(lua_State* L)
{
    //printf("ViewModel_gc: begin\n");
    //fflush(stdout);

    ViewModel* viewmodel = ToViewModel(L, 1);
    delete viewmodel->m_Instance;
    //printf("ViewModel_gc: %p : %p", viewmodel, viewmodel->m_Instance);
    //fflush(stdout);

    dmResource::Release(g_Factory, viewmodel->m_Resource);

    //printf("ViewModel_gc: end\n");
    //fflush(stdout);
    return 0;
}

static int ViewModelGetViewModel(lua_State* L)
{
    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
    RiveSceneData* resource = CompRiveGetRiveSceneData(component);
    rive::ViewModelInstanceRuntime* instance = CompRiveGetViewModelInstance(component);
    if (instance)
    {
        PushViewModel(L, resource, instance);
    }
    else
    {
        lua_pushnil(L);
    }

    return 1;
}

static int ViewModelGetProperty(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    ViewModel* viewmodel = CheckViewModel(L, 1);
    const char* property_path = luaL_checkstring(L, 2);;

    rive::ViewModelInstanceValueRuntime* property = viewmodel->m_Instance->property(property_path);
    if (!property)
    {
        return DM_LUA_ERROR("No property found with path '%s'", property_path);
    }


    printf("ViewModelGetProperty: %p : %p -> %p", viewmodel, viewmodel->m_Instance, property);
    PushViewModelProperty(L, viewmodel->m_Resource, property);
    return 1;
}


// *********************************************************************************

static const luaL_reg ViewModel_methods[] =
{
    {"get_property",       ViewModelGetProperty},
    {0,0}
};

static const luaL_reg ViewModel_meta[] =
{
    {"__tostring",  ViewModel_tostring},
    {"__eq",        ViewModel_eq},
    {"__gc",        ViewModel_gc},
    {0,0}
};

static const luaL_reg ViewModel_functions[] =
{
    {"get_viewmodel", ViewModelGetViewModel},
    {0, 0}
};

// *********************************************************************************

void PushViewModel(lua_State* L, RiveSceneData* resource, rive::ViewModelInstanceRuntime* instance)
{
    ViewModel* viewmodel = (ViewModel*)lua_newuserdata(L, sizeof(ViewModel));
    viewmodel->m_Resource = resource;
    viewmodel->m_Instance = instance;
    luaL_getmetatable(L, SCRIPT_TYPE_NAME_VIEWMODEL);
    lua_setmetatable(L, -2);
    dmResource::IncRef(g_Factory, resource);
}

ViewModel* CheckViewModel(lua_State* L, int index)
{
    ViewModel* viewmodel = (ViewModel*)dmScript::CheckUserType(L, index, TYPE_HASH_VIEWMODEL, 0);
    return viewmodel;
}

void ScriptInitializeViewModel(lua_State* L, dmResource::HFactory factory)
{
    int top = lua_gettop(L);

    g_Factory = factory;

    TYPE_HASH_VIEWMODEL = dmScript::RegisterUserType(L, SCRIPT_TYPE_NAME_VIEWMODEL, ViewModel_methods, ViewModel_meta);

    luaL_register(L, "rive", ViewModel_functions);
    lua_pop(L, 1);

    assert(top == lua_gettop(L));
}

}

#endif // DM_RIVE_UNSUPPORTED
