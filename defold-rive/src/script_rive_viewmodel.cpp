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

#include "script_defold.h"
#include <rive/lua/rive_lua_libs.hpp>
#include <rive_lua_wrapper.h>

namespace dmRive
{

static dmResource::HFactory g_Factory = 0;

static const char*  SCRIPT_TYPE_NAME_VIEWMODEL = rive::ScriptedViewModel::luaName;
static char SCRIPT_TYPE_VIEWMODEL_METATABLE_NAME[128];

// *********************************************************************************

static rive::ScriptedViewModel* ToViewModel(lua_State* L, int index)
{
    return (rive::ScriptedViewModel*)dmRive::ToUserType(L, index, SCRIPT_TYPE_VIEWMODEL_METATABLE_NAME);
}

rive::ScriptedViewModel* CheckViewModel(lua_State* L, int index)
{
    return (rive::ScriptedViewModel*)dmRive::CheckUserType(L, index, SCRIPT_TYPE_VIEWMODEL_METATABLE_NAME);
}

static int ViewModel_tostring(lua_State* L)
{
    rive::ScriptedViewModel* viewmodel = CheckViewModel(L, 1);
    lua_pushfstring(L, "rive.viewmodel(%p)", viewmodel);
    return 1;
}

static int ViewModel_gc(lua_State* L)
{
    rive::ScriptedViewModel* viewmodel = CheckViewModel(L, 1);
    viewmodel->~ScriptedViewModel();
    return 0;
}

static int ViewModel_index(lua_State* L)
{
    rive::ScriptedViewModel* viewmodel = CheckViewModel(L, 1);
    const char* name = luaL_checkstring(L, 2);
    return viewmodel->pushValue(name);
}

// static int ViewModelGetViewModel(lua_State* L)
// {
//     RiveComponent* component = 0;
//     dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
//     RiveSceneData* resource = CompRiveGetRiveSceneData(component);
//     rive::ViewModelInstanceRuntime* instance = CompRiveGetViewModelInstance(component);
//     if (instance)
//     {
//         PushViewModel(L, resource, instance);
//     }
//     else
//     {
//         lua_pushnil(L);
//     }

//     return 1;
// }

// static int ViewModelGetProperty(lua_State* L)
// {
//     DM_LUA_STACK_CHECK(L, 1);
//     ViewModel* viewmodel = CheckViewModel(L, 1);
//     const char* property_path = luaL_checkstring(L, 2);;

//     rive::ViewModelInstanceValueRuntime* property = viewmodel->m_Instance->property(property_path);
//     if (!property)
//     {
//         return DM_LUA_ERROR("No property found with path '%s'", property_path);
//     }


//     printf("ViewModelGetProperty: %p : %p -> %p", viewmodel, viewmodel->m_Instance, property);
//     PushViewModelProperty(L, viewmodel->m_Resource, property);
//     return 1;
// }


// *********************************************************************************

static const luaL_reg ViewModel_methods[] =
{
    //{"get_property",       ViewModelGetProperty},
    {0,0}
};

static const luaL_reg ViewModel_meta[] =
{
    {"__tostring",  ViewModel_tostring},
    {"__gc",        ViewModel_gc},
    {"__index",     ViewModel_index},
    {0,0}
};

void ScriptInitializeViewModel(lua_State* L, dmResource::HFactory factory)
{
    int top = lua_gettop(L);

    g_Factory = factory;

    GetMetaTableName(rive::ScriptedViewModel::luaName, SCRIPT_TYPE_VIEWMODEL_METATABLE_NAME, sizeof(SCRIPT_TYPE_VIEWMODEL_METATABLE_NAME));
    dmRive::RegisterUserType(L, SCRIPT_TYPE_NAME_VIEWMODEL, SCRIPT_TYPE_VIEWMODEL_METATABLE_NAME, ViewModel_methods, ViewModel_meta);

    assert(top == lua_gettop(L));
}

}

#endif // DM_RIVE_UNSUPPORTED
