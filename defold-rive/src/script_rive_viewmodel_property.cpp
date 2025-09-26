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

static const char*  SCRIPT_TYPE_NAME_VIEWMODEL_PROPERTY = "ViewModelProperty";
static uint32_t     TYPE_HASH_VIEWMODEL_PROPERTY = 0;

// *********************************************************************************

static bool IsViewModelProperty(lua_State* L, int index)
{
    return dmScript::GetUserType(L, index) == TYPE_HASH_VIEWMODEL_PROPERTY;
}

static ViewModelProperty* ToViewModelProperty(lua_State* L, int index)
{
    return (ViewModelProperty*)dmScript::ToUserType(L, index, TYPE_HASH_VIEWMODEL_PROPERTY);
}

static const char* DataTypeToString(rive::DataType type)
{
    #define DATATYPE_CASE(_NAME) case rive::DataType:: _NAME : return # _NAME;
    switch(type)
    {
        DATATYPE_CASE(none);
        DATATYPE_CASE(string);
        DATATYPE_CASE(number);
        DATATYPE_CASE(boolean);
        DATATYPE_CASE(color);
        DATATYPE_CASE(list);
        DATATYPE_CASE(enumType);
        DATATYPE_CASE(trigger);
        DATATYPE_CASE(viewModel);
        DATATYPE_CASE(integer);
        DATATYPE_CASE(symbolListIndex);
        DATATYPE_CASE(assetImage);
        DATATYPE_CASE(artboard);
        DATATYPE_CASE(input);
        default: return "unknown";
    };
    #undef DATATYPE_CASE
}

static int ViewModelProperty_tostring(lua_State* L)
{
    ViewModelProperty* prop = ToViewModelProperty(L, 1);
    if (!prop)
    {
        lua_pushstring(L, "no valid pointer!");
        return 1;
    }

    const char* typestr = DataTypeToString(prop->m_Instance->dataType());

    lua_pushfstring(L, "rive.viewmodelprop(%p : '%s', '%s')",
            prop, prop->m_Instance ? prop->m_Instance->name().c_str() : "null", typestr);
    return 1;
}

static int ViewModelProperty_eq(lua_State* L)
{
    ViewModelProperty* v1 = ToViewModelProperty(L, 1);
    ViewModelProperty* v2 = ToViewModelProperty(L, 2);
    lua_pushboolean(L, v1 && v2 && v1 == v2);
    return 1;
}

static int ViewModelProperty_gc(lua_State* L)
{
    //printf("ViewModelProperty_gc: begin\n");
    //fflush(stdout);

    ViewModelProperty* prop = ToViewModelProperty(L, 1);
    //printf("ViewModelProperty_gc: %p : %p\n", prop, prop->m_Instance);
    //fflush(stdout);

    dmResource::Release(g_Factory, prop->m_Resource);

    //printf("ViewModelProperty_gc: end\n");
    //fflush(stdout);
    return 0;
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

// *********************************************************************************

static const luaL_reg ViewModelProperty_methods[] =
{
    {0,0}
};

static const luaL_reg ViewModelProperty_meta[] =
{
    {"__tostring",  ViewModelProperty_tostring},
    {"__eq",        ViewModelProperty_eq},
    {"__gc",        ViewModelProperty_gc},
    {0,0}
};

// *********************************************************************************

void PushViewModelProperty(lua_State* L, RiveSceneData* resource, rive::ViewModelInstanceValueRuntime* instance)
{
    ViewModelProperty* viewmodel = (ViewModelProperty*)lua_newuserdata(L, sizeof(ViewModelProperty));
    viewmodel->m_Resource = resource;
    viewmodel->m_Instance = instance;
    luaL_getmetatable(L, SCRIPT_TYPE_NAME_VIEWMODEL_PROPERTY);
    lua_setmetatable(L, -2);
    dmResource::IncRef(g_Factory, resource);
}

ViewModelProperty* CheckViewModelProperty(lua_State* L, int index)
{
    ViewModelProperty* viewmodel = (ViewModelProperty*)dmScript::CheckUserType(L, index, TYPE_HASH_VIEWMODEL_PROPERTY, 0);
    return viewmodel;
}

void ScriptInitializeViewModelProperty(lua_State* L, dmResource::HFactory factory)
{
    int top = lua_gettop(L);

    g_Factory = factory;

    TYPE_HASH_VIEWMODEL_PROPERTY = dmScript::RegisterUserType(L, SCRIPT_TYPE_NAME_VIEWMODEL_PROPERTY, ViewModelProperty_methods, ViewModelProperty_meta);

    assert(top == lua_gettop(L));
}

}

#endif // DM_RIVE_UNSUPPORTED
