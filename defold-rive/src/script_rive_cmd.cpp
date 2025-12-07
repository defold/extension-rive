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

#include <common/commands.h>

#include <rive/shapes/paint/color.hpp>

namespace dmRive
{

static dmResource::HFactory g_ResourceFactory = 0;

static bool CheckBoolean(lua_State* L, int index)
{
    if (lua_isboolean(L, index))
    {
        return lua_toboolean(L, index);
    }
    return luaL_error(L, "Argument %d must be a boolean", index);
}

template<typename INTEGER>
static void CheckStringOrInteger(lua_State* L, int index, const char** string, INTEGER* integer)
{
    if (lua_type(L, index) == LUA_TSTRING)
        *string = luaL_checkstring(L, index);
    else
        *integer = (INTEGER)luaL_checkinteger(L, index);
}

static int Script_instantiateBlankViewModelInstance(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);

    rive::ArtboardHandle artboard = 0;
    const char* viewmodel_name = 0;
    CheckStringOrInteger(L, 2, &viewmodel_name, &artboard);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    rive::ViewModelInstanceHandle handle = 0;
    if (viewmodel_name)
        handle = queue->instantiateBlankViewModelInstance(file, viewmodel_name);
    else
        handle = queue->instantiateBlankViewModelInstance(file, artboard);

    lua_pushinteger(L, (lua_Integer)handle);
    return 1;
}

static int Script_instantiateDefaultViewModelInstance(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);

    rive::ArtboardHandle artboard = 0;
    const char* viewmodel_name = 0;
    CheckStringOrInteger(L, 2, &viewmodel_name, &artboard);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    rive::ViewModelInstanceHandle handle = 0;
    if (viewmodel_name)
        handle = queue->instantiateDefaultViewModelInstance(file, viewmodel_name);
    else
        handle = queue->instantiateDefaultViewModelInstance(file, artboard);

    lua_pushinteger(L, (lua_Integer)handle);
    return 1;
}

static int Script_instantiateViewModelInstanceNamed(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);

    rive::ArtboardHandle artboard = 0;
    const char* viewmodel_name = 0;
    CheckStringOrInteger(L, 2, &viewmodel_name, &artboard);

    const char* view_model_instance_name = luaL_checkstring(L, 3);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    rive::ViewModelInstanceHandle handle = 0;
    if (viewmodel_name)
        handle = queue->instantiateViewModelInstanceNamed(file, viewmodel_name, view_model_instance_name);
    else
        handle = queue->instantiateViewModelInstanceNamed(file, artboard, view_model_instance_name);

    lua_pushinteger(L, (lua_Integer)handle);
    return 1;
}

static int Script_referenceNestedViewModelInstance(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* path = luaL_checkstring(L, 2);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    handle = queue->referenceNestedViewModelInstance(handle, path);
    lua_pushinteger(L, (lua_Integer)handle);
    return 1;
}

static int Script_referenceListViewModelInstance(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* path = luaL_checkstring(L, 2);
    int index = luaL_checkinteger(L, 3);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    handle = queue->referenceListViewModelInstance(handle, path, index);
    lua_pushinteger(L, (lua_Integer)handle);
    return 1;
}

static int Script_setViewModelInstanceNestedViewModel(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    rive::ViewModelInstanceHandle value  = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 3);
    const char* path = luaL_checkstring(L, 2);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->setViewModelInstanceNestedViewModel(handle, path, value);
    return 0;
}

static int Script_insertViewModelInstanceListViewModel(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    rive::ViewModelInstanceHandle value  = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 3);
    const char* path = luaL_checkstring(L, 2);
    int index = luaL_checkinteger(L, 4);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->insertViewModelInstanceListViewModel(handle, path, value, index);
    return 0;
}

static int Script_appendViewModelInstanceListViewModel(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    rive::ViewModelInstanceHandle value  = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 3);
    const char* path = luaL_checkstring(L, 2);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->appendViewModelInstanceListViewModel(handle, path, value);
    return 0;
}

static int Script_removeViewModelInstanceListViewModelIndex(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* path = luaL_checkstring(L, 2);
    int index = luaL_checkinteger(L, 3);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->removeViewModelInstanceListViewModel(handle, path, index, RIVE_NULL_HANDLE);
    return 0;
}

static int Script_removeViewModelInstanceListViewModel(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    rive::ViewModelInstanceHandle value = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 3);
    const char* path = luaL_checkstring(L, 2);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->removeViewModelInstanceListViewModel(handle, path, value);
    return 0;
}

static int Script_deleteViewModelInstance(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->deleteViewModelInstance(handle);
    return 0;
}

// *****************************************************************************************


static int Script_bindViewModelInstance(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::StateMachineHandle state_machine = (rive::StateMachineHandle)luaL_checkinteger(L, 1);
    rive::ViewModelInstanceHandle view_model = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 2);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->bindViewModelInstance(state_machine, view_model);
    return 0;
}


// *****************************************************************************************


static int Script_fireViewModelTrigger(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* path = luaL_checkstring(L, 2);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->fireViewModelTrigger(handle, path);
    return 0;
}

static int Script_setViewModelInstanceBool(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    bool value  = CheckBoolean(L, 3);
    const char* path = luaL_checkstring(L, 2);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->setViewModelInstanceBool(handle, path, value);
    return 0;
}

static int Script_setViewModelInstanceNumber(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    lua_Number value  = luaL_checknumber(L, 3);
    const char* path = luaL_checkstring(L, 2);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->setViewModelInstanceNumber(handle, path, value);
    return 0;
}

static int Script_setViewModelInstanceColor(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* path = luaL_checkstring(L, 2);
    dmVMath::Vector4* color = dmScript::CheckVector4(L, 3);
    rive::ColorInt value = rive::colorARGB(255 * color->getW(), 255 * color->getX(), 255 * color->getY(), 255 * color->getZ());

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->setViewModelInstanceColor(handle, path, value);
    return 0;
}

static int Script_setViewModelInstanceEnum(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* path = luaL_checkstring(L, 2);
    const char* value = luaL_checkstring(L, 3);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->setViewModelInstanceEnum(handle, path, value);
    return 0;
}

static int Script_setViewModelInstanceString(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* path = luaL_checkstring(L, 2);
    const char* value = luaL_checkstring(L, 3);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->setViewModelInstanceString(handle, path, value);
    return 0;
}

static int Script_setViewModelInstanceImage(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    rive::RenderImageHandle value  = (rive::RenderImageHandle)luaL_checkinteger(L, 3);
    const char* path = luaL_checkstring(L, 2);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->setViewModelInstanceImage(handle, path, value);
    return 0;
}

static int Script_setViewModelInstanceArtboard(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    rive::ArtboardHandle value  = (rive::ArtboardHandle)luaL_checkinteger(L, 3);
    const char* path = luaL_checkstring(L, 2);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->setViewModelInstanceArtboard(handle, path, value);
    return 0;
}

// *****************************************************************************************

static int Script_decodeImage(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    size_t data_length = 0;
    const uint8_t* data = (const uint8_t*)luaL_checklstring(L, 1, &data_length);
    std::vector<uint8_t> imageEncodedBytes(data, data + data_length);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    rive::RenderImageHandle image = queue->decodeImage(imageEncodedBytes);

    lua_pushinteger(L, (lua_Integer)image);
    return 1;
}

static int Script_deleteImage(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::RenderImageHandle handle = (rive::RenderImageHandle)luaL_checkinteger(L, 1);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->deleteImage(handle);
    return 0;
}

static const luaL_reg RIVE_COMMAND_FUNCTIONS[] =
{
    {"instantiateBlankViewModelInstance", Script_instantiateBlankViewModelInstance},
    {"instantiateDefaultViewModelInstance", Script_instantiateDefaultViewModelInstance},
    {"instantiateViewModelInstanceNamed", Script_instantiateViewModelInstanceNamed},
    {"referenceNestedViewModelInstance", Script_referenceNestedViewModelInstance},
    {"referenceListViewModelInstance", Script_referenceListViewModelInstance},

    {"setViewModelInstanceNestedViewModel",     Script_setViewModelInstanceNestedViewModel},
    {"insertViewModelInstanceListViewModel",    Script_insertViewModelInstanceListViewModel},
    {"appendViewModelInstanceListViewModel",    Script_appendViewModelInstanceListViewModel},

    {"removeViewModelInstanceListViewModelIndex", Script_removeViewModelInstanceListViewModelIndex},
    {"removeViewModelInstanceListViewModel", Script_removeViewModelInstanceListViewModel},

    {"bindViewModelInstance", Script_bindViewModelInstance},

    {"fireViewModelTrigger",        Script_fireViewModelTrigger},
    {"setViewModelInstanceBool",    Script_setViewModelInstanceBool},
    {"setViewModelInstanceNumber",  Script_setViewModelInstanceNumber},
    {"setViewModelInstanceColor",   Script_setViewModelInstanceColor},
    {"setViewModelInstanceEnum",    Script_setViewModelInstanceEnum},
    {"setViewModelInstanceString",  Script_setViewModelInstanceString},
    {"setViewModelInstanceImage",   Script_setViewModelInstanceImage},
    {"setViewModelInstanceArtboard",Script_setViewModelInstanceArtboard},

    {"deleteViewModelInstance", Script_deleteViewModelInstance},

    {"decodeImage", Script_decodeImage},
    {"deleteImage", Script_deleteImage},

    {0, 0}
};

void ScriptCmdRegister(struct lua_State* L, dmResource::HFactory factory)
{
    lua_newtable(L);
    luaL_register(L, 0, RIVE_COMMAND_FUNCTIONS);
    lua_setfield(L, -2, "cmd");

    g_ResourceFactory = factory;
}

void ScriptCmdUnregister(struct lua_State* L, dmResource::HFactory factory)
{
    g_ResourceFactory = 0;
}

} // namespace dmRive

#endif // DM_RIVE_UNSUPPORTED
