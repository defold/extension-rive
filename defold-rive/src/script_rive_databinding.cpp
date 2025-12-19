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

#include "comp_rive.h"
#include "comp_rive_private.h"
#include "res_rive_data.h"

#include <rive/data_bind/data_values/data_type.hpp>

namespace dmRive
{

static dmResource::HFactory g_ResourceFactory = 0;

static int CreateViewModelInstanceRuntime(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 2);

    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);

    dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);

    uint32_t handle = dmRive::CompRiveCreateViewModelInstanceRuntime(component, name_hash);
    if (handle != dmRive::INVALID_HANDLE)
    {
        lua_pushinteger(L, handle);
        lua_pushnil(L);
    }
    else
    {
        lua_pushnil(L);
        char msg[256];
        dmSnPrintf(msg, sizeof(msg), "Failed to create view model instance runtime with name '%s'", dmHashReverseSafe64(name_hash));
        lua_pushstring(L, msg);
    }
    return 2;
}

static int DestroyViewModelInstanceRuntime(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);

    uint32_t handle = luaL_checkinteger(L, 2);
    dmRive::CompRiveDestroyViewModelInstanceRuntime(component, handle);
    return 0;
}

static int SetViewModelInstanceRuntime(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);

    uint32_t handle = luaL_checkinteger(L, 2);
    bool result = dmRive::CompRiveSetViewModelInstanceRuntime(component, handle);
    if (!result)
    {
        return DM_LUA_ERROR("Could not set view model instance runtime with handle '%u'", handle);
    }

    return 0;
}

static int GetViewModelInstanceRuntime(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, 0);
    uint32_t handle = CompRiveGetViewModelInstanceRuntime(component);
    lua_pushinteger(L, handle);
    return 1;
}

#define CHECK_BOOLEAN(PATH, INDEX) \
    if (!lua_isboolean(L, INDEX)) { \
        char buffer[1024]; \
        lua_pushvalue(L, INDEX); \
        dmSnPrintf(buffer, sizeof(buffer), "Property '%s' is not a boolean: '%s'", PATH, lua_tostring(L, -1)); \
        lua_pop(L, lua_gettop(L) - top); \
        return DM_LUA_ERROR("%s", buffer); \
    }

#define CHECK_RESULT(RESULT, URL, PATH, INDEX) \
    if (!result) { \
        char buffer[1024]; \
        lua_pushvalue(L, INDEX); \
        dmSnPrintf(buffer, sizeof(buffer), "%s has no property with path '%s' that accepts type '%s' (value='%s')", dmScript::UrlToString(URL, buffer, sizeof(buffer)), PATH, lua_typename(L, lua_type(L, -1)), lua_tostring(L, -1)); \
        lua_pop(L, lua_gettop(L) - top); \
        return DM_LUA_ERROR("%s", buffer); \
    }

static int SetProperties(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    int top = lua_gettop(L);

    dmMessage::URL url;
    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, &url);

    uint32_t handle = luaL_checkinteger(L, 2);

    luaL_checktype(L, 3, LUA_TTABLE);
    lua_pushvalue(L, 3);
    lua_pushnil(L);
    while (lua_next(L, -2)) {
        lua_pushvalue(L, -2); // Add a duplicate of the key, as we do a lua_tostring on it

        // Note: This is a "path", as it may refer to a property within a nested structure of view model instance runtimes
        const char* path = lua_tostring(L, -1); // the key

        // Check what property type is wanted
        rive::DataType data_type = rive::DataType::none;
        bool result = dmRive::GetPropertyDataType(component, handle, path, &data_type);
        CHECK_RESULT(result, &url, path, -2);

        if (data_type == rive::DataType::number)
        {
            float value = lua_tonumber(L, -2);
            bool result = dmRive::CompRiveRuntimeSetPropertyF32(component, handle, path, value);
            CHECK_RESULT(result, &url, path, -2);
        }
        else if (data_type == rive::DataType::string)
        {
            const char* value = luaL_checkstring(L, -2);
            bool result = dmRive::CompRiveRuntimeSetPropertyString(component, handle, path, value);
            CHECK_RESULT(result, &url, path, -2);
        }
        else if (data_type == rive::DataType::boolean)
        {
            CHECK_BOOLEAN(path, -2);
            bool value = lua_toboolean(L, -2);
            bool result = dmRive::CompRiveRuntimeSetPropertyBool(component, handle, path, value);
            CHECK_RESULT(result, &url, path, -2);
        }
        else if (data_type == rive::DataType::trigger)
        {
            CHECK_BOOLEAN(path, -2);
            bool value = lua_toboolean(L, -2);
            // Trigger doesn't really use a value.
            // But no value would mean nil, and nil would mean it's not in the table to begin with.
            // So we use a bool as the "dummy" value!
            // However, it would feel strange to trigger() on a false value :/
            if (value)
            {
                bool result = dmRive::CompRiveRuntimeSetPropertyTrigger(component, handle, path);
                CHECK_RESULT(result, &url, path, -2);
            }
        }
        else if (data_type == rive::DataType::color)
        {
            dmVMath::Vector4* color = dmScript::CheckVector4(L, -2);
            bool result = dmRive::CompRiveRuntimeSetPropertyColor(component, handle, path, color);
            CHECK_RESULT(result, &url, path, -2);
        }
        else if (data_type == rive::DataType::enumType)
        {
            const char* value = luaL_checkstring(L, -2);
            result = dmRive::CompRiveRuntimeSetPropertyEnum(component, handle, path, value);
            CHECK_RESULT(result, &url, path, -2);
        }
        else if (data_type == rive::DataType::assetImage)
        {
            size_t data_length = 0;
            const char* data = luaL_checklstring(L, -2, &data_length);

            RiveSceneData* rive_scene_data = dmRive::CompRiveGetRiveSceneData(component);
            rive::RenderImage* image = ResRiveDataCreateRenderImage(g_ResourceFactory, rive_scene_data, (uint8_t*)data, (uint32_t)data_length);
            result = dmRive::CompRiveRuntimeSetPropertyImage(component, handle, path, image);
            CHECK_RESULT(result, &url, path, -2);
        }
        else
        {
            dmLogWarning("Datatype %d is not yet supported (path: '%s')", (int)data_type, path);
        }

        lua_pop(L, 2);
    }
    lua_pop(L, 1);

    return 0;
}

static int GetProperty(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    int top = lua_gettop(L);

    dmMessage::URL url;
    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, &url);

    uint32_t handle = luaL_checkinteger(L, 2);
    if (handle == dmRive::INVALID_HANDLE)
    {
        return DM_LUA_ERROR("Invalid handle: %d (x%08x) at index %d", (int)handle, handle, 2);
    }

    // Note: This is a "path", as it may refer to a property within a nested structure of view model instance runtimes
    const char* path = lua_tostring(L, 3); // the path

    rive::DataType data_type = rive::DataType::none;
    bool result = dmRive::GetPropertyDataType(component, handle, path, &data_type);
    CHECK_RESULT(result, &url, path, -2);

    if (data_type == rive::DataType::number)
    {
        float value = false;
        bool result = dmRive::CompRiveRuntimeGetPropertyF32(component, handle, path, &value);
        CHECK_RESULT(result, &url, path, -2);
        lua_pushnumber(L, value);
    }
    else if (data_type == rive::DataType::string)
    {
        const char* value = 0;
        bool result = dmRive::CompRiveRuntimeGetPropertyString(component, handle, path, &value);
        CHECK_RESULT(result, &url, path, -2);
        lua_pushstring(L, value);
    }
    else if (data_type == rive::DataType::boolean)
    {
        bool value = false;
        bool result = dmRive::CompRiveRuntimeGetPropertyBool(component, handle, path, &value);
        CHECK_RESULT(result, &url, path, -2);
        lua_pushboolean(L, value);
    }
    else if (data_type == rive::DataType::enumType)
    {
        const char* value = 0;
        bool result = dmRive::CompRiveRuntimeGetPropertyEnum(component, handle, path, &value);
        CHECK_RESULT(result, &url, path, -2);
        lua_pushstring(L, value);
    }
    else if(data_type == rive::DataType::color)
    {
        dmVMath::Vector4 value(-1, -1, -1, -1);
        bool result = dmRive::CompRiveRuntimeGetPropertyColor(component, handle, path, &value);
        CHECK_RESULT(result, &url, path, -2);
        dmScript::PushVector4(L, value);
    }
    else if(data_type == rive::DataType::trigger)
    {
        return DM_LUA_ERROR("Cannot get trigger property value as it is input-only (path: '%s')", path);
    }
    else
    {
        return DM_LUA_ERROR("Getting data type %d is not supported (path: '%s')", (int)data_type, path);
    }
    return 1;
}

static int ListAddInstanceInternal(lua_State* L, bool add, int index)
{
    DM_LUA_STACK_CHECK(L, 0);

    dmMessage::URL url;
    RiveComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, dmRive::RIVE_MODEL_EXT, 0, (void**)&component, &url);

    uint32_t handle = luaL_checkinteger(L, 2);
    if (handle == dmRive::INVALID_HANDLE)
    {
        return DM_LUA_ERROR("Invalid handle: %d (x%08x) at index %d", (int)handle, handle, 2);
    }

    // Note: This is a "path", as it may refer to a property within a nested structure of view model instance runtimes
    const char* path = lua_tostring(L, 3); // the path

    uint32_t instance = luaL_checkinteger(L, 4);
    if (instance == dmRive::INVALID_HANDLE)
    {
        return DM_LUA_ERROR("Invalid handle: %d (x%08x) at index %d", (int)instance, instance, 3);
    }

    bool result;
    if (add)
        result = dmRive::CompRiveRuntimeListAddInstance(component, handle, path, instance);
    else
        result = dmRive::CompRiveRuntimeListRemoveInstance(component, handle, path, instance);
    if (!result)
    {
        if (add)
        {
            return DM_LUA_ERROR("Failed adding instance to list %s", path);
        }
        else
        {
            return DM_LUA_ERROR("Failed removing instance from list %s", path);
        }
    }

    return 0;
}

static int ListAddInstance(lua_State* L)
{
    return ListAddInstanceInternal(L, true, -1);
}

static int ListRemoveInstance(lua_State* L)
{
    return ListAddInstanceInternal(L, false, -1);
}


#undef CHECK_RESULT
#undef CHECK_BOOLEAN

static const luaL_reg RIVE_DATABIND_FUNCTIONS[] =
{
    {"create_view_model_instance_runtime",  CreateViewModelInstanceRuntime},
    {"destroy_view_model_instance_runtime", DestroyViewModelInstanceRuntime},
    {"set_view_model_instance_runtime",     SetViewModelInstanceRuntime},
    {"get_view_model_instance_runtime",     GetViewModelInstanceRuntime},
    {"set_properties",                      SetProperties},
    {"get_property",                        GetProperty},
    {"list_add_instance",                   ListAddInstance},
    {"list_remove_instance",                ListRemoveInstance},
    {0, 0}
};

void ScriptInitializeDataBinding(struct lua_State* L, dmResource::HFactory factory)
{
    lua_newtable(L);
    luaL_register(L, 0, RIVE_DATABIND_FUNCTIONS);
    lua_setfield(L, -2, "databind");

    g_ResourceFactory = factory;
}

} // namespace dmRive

#endif // DM_RIVE_UNSUPPORTED
