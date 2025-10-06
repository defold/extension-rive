// // Copyright 2021 The Defold Foundation
// // Licensed under the Defold License version 1.0 (the "License"); you may not use
// // this file except in compliance with the License.
// //
// // You may obtain a copy of the License, together with FAQs at
// // https://www.defold.com/license
// //
// // Unless required by applicable law or agreed to in writing, software distributed
// // under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// // CONDITIONS OF ANY KIND, either express or implied. See the License for the
// // specific language governing permissions and limitations under the License.


// #if !defined(DM_RIVE_UNSUPPORTED)

// #include <stdint.h>

// // #include <dmsdk/gameobject/script.h>
// // #include <dmsdk/gamesys/script.h>
// #include <dmsdk/script/script.h>
// #include <dmsdk/resource/resource.h>

// #include "comp_rive.h"
// #include "comp_rive_private.h"
// #include "res_rive_data.h"
// #include "script_rive_private.h"

// #include <rive/viewmodel/runtime/viewmodel_instance_runtime.hpp>
// #include <rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp>
// #include <rive/viewmodel/runtime/viewmodel_instance_number_runtime.hpp>

// namespace dmRive
// {

// static dmResource::HFactory g_Factory = 0;

// static const char*  SCRIPT_TYPE_NAME_VIEWMODEL_PROPERTY = "ViewModelProperty";
// static uint32_t     TYPE_HASH_VIEWMODEL_PROPERTY = 0;

// // *********************************************************************************

// static bool IsViewModelProperty(lua_State* L, int index)
// {
//     return dmScript::GetUserType(L, index) == TYPE_HASH_VIEWMODEL_PROPERTY;
// }

// static ViewModelProperty* ToViewModelProperty(lua_State* L, int index)
// {
//     return (ViewModelProperty*)dmScript::ToUserType(L, index, TYPE_HASH_VIEWMODEL_PROPERTY);
// }

// void PushViewModelProperty(lua_State* L, RiveSceneData* resource, rive::ViewModelInstanceValueRuntime* instance)
// {
//     ViewModelProperty* viewmodel = (ViewModelProperty*)lua_newuserdata(L, sizeof(ViewModelProperty));
//     viewmodel->m_Resource = resource;
//     viewmodel->m_Instance = instance;
//     luaL_getmetatable(L, SCRIPT_TYPE_NAME_VIEWMODEL_PROPERTY);
//     lua_setmetatable(L, -2);
//     dmResource::IncRef(g_Factory, resource);
// }

// ViewModelProperty* CheckViewModelProperty(lua_State* L, int index)
// {
//     ViewModelProperty* viewmodel = (ViewModelProperty*)dmScript::CheckUserType(L, index, TYPE_HASH_VIEWMODEL_PROPERTY, 0);
//     return viewmodel;
// }

// // *********************************************************************************

// static const char* DataTypeToString(rive::DataType type)
// {
//     #define DATATYPE_CASE(_NAME) case rive::DataType:: _NAME : return # _NAME;
//     switch(type)
//     {
//         DATATYPE_CASE(none);
//         DATATYPE_CASE(string);
//         DATATYPE_CASE(number);
//         DATATYPE_CASE(boolean);
//         DATATYPE_CASE(color);
//         DATATYPE_CASE(list);
//         DATATYPE_CASE(enumType);
//         DATATYPE_CASE(trigger);
//         DATATYPE_CASE(viewModel);
//         DATATYPE_CASE(integer);
//         DATATYPE_CASE(symbolListIndex);
//         DATATYPE_CASE(assetImage);
//         DATATYPE_CASE(artboard);
//         DATATYPE_CASE(input);
//         default: return "unknown";
//     };
//     #undef DATATYPE_CASE
// }

// static int ViewModelProperty_tostring(lua_State* L)
// {
//     ViewModelProperty* prop = ToViewModelProperty(L, 1);
//     if (!prop)
//     {
//         lua_pushstring(L, "no valid pointer!");
//         return 1;
//     }

//     const char* typestr = DataTypeToString(prop->m_Instance->dataType());

//     lua_pushfstring(L, "rive.viewmodelprop(%p : '%s', '%s')",
//             prop, prop->m_Instance ? prop->m_Instance->name().c_str() : "null", typestr);
//     return 1;
// }

// static int ViewModelProperty_eq(lua_State* L)
// {
//     ViewModelProperty* v1 = ToViewModelProperty(L, 1);
//     ViewModelProperty* v2 = ToViewModelProperty(L, 2);
//     lua_pushboolean(L, v1 && v2 && v1 == v2);
//     return 1;
// }

// static int ViewModelProperty_gc(lua_State* L)
// {
//     ViewModelProperty* prop = ToViewModelProperty(L, 1);
//     dmResource::Release(g_Factory, prop->m_Resource);
//     return 0;
// }

// static int ViewModelPropertyName(lua_State* L)
// {
//     ViewModelProperty* prop = CheckViewModelProperty(L, 1);
//     lua_pushstring(L, prop->m_Instance->name().c_str());
//     return 1;
// }

// static int ViewModelPropertyType(lua_State* L)
// {
//     ViewModelProperty* prop = CheckViewModelProperty(L, 1);
//     lua_pushinteger(L, (int)prop->m_Instance->dataType());
//     return 1;
// }

// static dmVMath::Vector4 GetColor(rive::ViewModelInstanceValueRuntime* _prop)
// {
//     rive::ViewModelInstanceColorRuntime* prop = (rive::ViewModelInstanceColorRuntime*)_prop;

//     uint32_t argb = (uint32_t)prop->value();
//     float a = ((argb >> 24) & 0xFF) / 255.0f;
//     float r = ((argb >> 16) & 0xFF) / 255.0f;
//     float g = ((argb >>  8) & 0xFF) / 255.0f;
//     float b = ((argb >>  0) & 0xFF) / 255.0f;
//     return dmVMath::Vector4(r, g, b, a);
// }

// static void SetArtboard(rive::ViewModelInstanceValueRuntime* _prop, rive::ArtboardInstance* instance)
// {
//     rive::ViewModelInstanceArtboardRuntime* prop = (rive::ViewModelInstanceArtboardRuntime*)_prop;
//     prop->value(instance);
// }

// static int ViewModelPropertyGet(lua_State* L)
// {
//     DM_LUA_STACK_CHECK(L, 1);

//     ViewModelProperty* prop = CheckViewModelProperty(L, 1);
//     rive::DataType type = prop->m_Instance->dataType();

//     switch(type)
//     {
//         case rive::DataType::number:    lua_pushnumber(L, ((rive::ViewModelInstanceNumberRuntime*)prop->m_Instance)->value()); break;
//         case rive::DataType::string:    lua_pushstring(L, ((rive::ViewModelInstanceStringRuntime*)prop->m_Instance)->value().c_str()); break;
//         case rive::DataType::enumType:  lua_pushstring(L, ((rive::ViewModelInstanceEnumRuntime*)prop->m_Instance)->value().c_str()); break;
//         case rive::DataType::boolean:   lua_pushboolean(L, ((rive::ViewModelInstanceBooleanRuntime*)prop->m_Instance)->value()); break;
//         case rive::DataType::color:     dmScript::PushVector4(L, GetColor(prop->m_Instance)); break;


//         // todo: support
//         case rive::DataType::list:
//         case rive::DataType::viewModel:
//         case rive::DataType::integer:
//         case rive::DataType::symbolListIndex:
//         case rive::DataType::assetImage:
//         case rive::DataType::input:

//         // not gettable
//         case rive::DataType::artboard:
//         case rive::DataType::trigger:
//         case rive::DataType::none:
//         default:
//             dmLogError("Property '%s': Type has no gettable value: %d (%s)",
//                         prop->m_Instance->name().c_str(), type, DataTypeToString(type));
//             lua_pushnil(L);
//     }

//     return 1;
// }

// static int ViewModelPropertySet(lua_State* L)
// {
//     DM_LUA_STACK_CHECK(L, 0);

//     ViewModelProperty* prop = CheckViewModelProperty(L, 1);
//     rive::DataType type = prop->m_Instance->dataType();

//     switch(type)
//     {
//         // case rive::DataType::number:    lua_pushnumber(L, ((rive::ViewModelInstanceNumberRuntime*)prop->m_Instance)->value()); break;
//         // case rive::DataType::string:    lua_pushstring(L, ((rive::ViewModelInstanceStringRuntime*)prop->m_Instance)->value().c_str()); break;
//         // case rive::DataType::enumType:  lua_pushstring(L, ((rive::ViewModelInstanceEnumRuntime*)prop->m_Instance)->value().c_str()); break;
//         // case rive::DataType::boolean:   lua_pushboolean(L, ((rive::ViewModelInstanceBooleanRuntime*)prop->m_Instance)->value()); break;
//         // case rive::DataType::color:     dmScript::PushVector4(L, GetColor(prop->m_Instance)); break;

//         case rive::DataType::artboard:
//             {
//                 Artboard* artboard = CheckArtboard(L, 2);
//                 SetArtboard(prop->m_Instance, artboard->m_Instance);
//             }
//             break;

//         // todo: support
//         case rive::DataType::list:
//         case rive::DataType::viewModel:
//         case rive::DataType::integer:
//         case rive::DataType::symbolListIndex:
//         case rive::DataType::assetImage:
//         case rive::DataType::input:

//         // not settable
//         case rive::DataType::trigger:
//         case rive::DataType::none:
//         default:
//             dmLogError("Property '%s': Type has no settable value: %d (%s)",
//                         prop->m_Instance->name().c_str(), type, DataTypeToString(type));
//     }

//     return 0;
// }

// // *********************************************************************************

// static const luaL_reg ViewModelProperty_methods[] =
// {
//     {"get",     ViewModelPropertyGet},
//     {"set",     ViewModelPropertySet},
//     {"name",    ViewModelPropertyName},
//     {"type",    ViewModelPropertyType},
//     {0,0}
// };

// static const luaL_reg ViewModelProperty_meta[] =
// {
//     {"__tostring",  ViewModelProperty_tostring},
//     {"__eq",        ViewModelProperty_eq},
//     {"__gc",        ViewModelProperty_gc},
//     {0,0}
// };

// // *********************************************************************************

// void ScriptInitializeViewModelProperty(lua_State* L, dmResource::HFactory factory)
// {
//     int top = lua_gettop(L);

//     g_Factory = factory;

//     TYPE_HASH_VIEWMODEL_PROPERTY = dmScript::RegisterUserType(L, SCRIPT_TYPE_NAME_VIEWMODEL_PROPERTY, ViewModelProperty_methods, ViewModelProperty_meta);

// // TODO: Set rive.DATATYPE_NONE = 0 etc for all data types!

//     assert(top == lua_gettop(L));
// }

// }

// #endif // DM_RIVE_UNSUPPORTED
