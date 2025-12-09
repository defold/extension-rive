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

static int Script_instantiateArtboardNamed(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);
    const char* viewmodel_name = luaL_checkstring(L, 2);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    rive::ArtboardHandle handle = queue->instantiateArtboardNamed(file, viewmodel_name);

    lua_pushinteger(L, (lua_Integer)handle);
    return 1;
}

static int Script_instantiateDefaultArtboard(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    rive::ArtboardHandle handle = queue->instantiateDefaultArtboard(file);

    lua_pushinteger(L, (lua_Integer)handle);
    return 1;
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

static int Script_swapViewModelInstanceListValues(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* path = luaL_checkstring(L, 2);
    int indexa = luaL_checkinteger(L, 3);
    int indexb = luaL_checkinteger(L, 4);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->swapViewModelInstanceListValues(handle, path, indexa, indexb);
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

static int Script_subscribeToViewModelProperty(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* path = luaL_checkstring(L, 2);
    rive::DataType type = (rive::DataType)luaL_checkinteger(L, 3);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->subscribeToViewModelProperty(handle, path, type);
    return 0;
}

static int Script_unsubscribeToViewModelProperty(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* path = luaL_checkstring(L, 2);
    rive::DataType type = (rive::DataType)luaL_checkinteger(L, 3);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->unsubscribeToViewModelProperty(handle, path, type);
    return 0;
}

// *****************************************************************************************

static int Script_loadFile(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    size_t data_length = 0;
    const uint8_t* data = (const uint8_t*)luaL_checklstring(L, 1, &data_length);
    std::vector<uint8_t> rivBytes(data, data + data_length);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    rive::FileHandle image = queue->loadFile(rivBytes);

    lua_pushinteger(L, (lua_Integer)image);
    return 1;
}

static int Script_deleteFile(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::FileHandle handle = (rive::FileHandle)luaL_checkinteger(L, 1);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->deleteFile(handle);
    return 0;
}

static int Script_decodeImage(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    size_t data_length = 0;
    const uint8_t* data = (const uint8_t*)luaL_checklstring(L, 1, &data_length);
    std::vector<uint8_t> encodedBytes(data, data + data_length);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    rive::RenderImageHandle image = queue->decodeImage(encodedBytes);

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

static int Script_decodeAudio(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    size_t data_length = 0;
    const uint8_t* data = (const uint8_t*)luaL_checklstring(L, 1, &data_length);
    std::vector<uint8_t> encodedBytes(data, data + data_length);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    rive::AudioSourceHandle handle = queue->decodeAudio(encodedBytes);

    lua_pushinteger(L, (lua_Integer)handle);
    return 1;
}

static int Script_deleteAudio(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::AudioSourceHandle handle = (rive::AudioSourceHandle)luaL_checkinteger(L, 1);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->deleteAudio(handle);
    return 0;
}

static int Script_decodeFont(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    size_t data_length = 0;
    const uint8_t* data = (const uint8_t*)luaL_checklstring(L, 1, &data_length);
    std::vector<uint8_t> encodedBytes(data, data + data_length);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    rive::FontHandle handle = queue->decodeFont(encodedBytes);

    lua_pushinteger(L, (lua_Integer)handle);
    return 1;
}

static int Script_deleteFont(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::FontHandle handle = (rive::FontHandle)luaL_checkinteger(L, 1);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->deleteFont(handle);
    return 0;
}

static int Script_addGlobalImageAsset(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    const char* name = luaL_checkstring(L, 1);
    rive::RenderImageHandle handle = (rive::RenderImageHandle)luaL_checkinteger(L, 2);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->addGlobalImageAsset(name, handle);
    return 0;
}

static int Script_removeGlobalImageAsset(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    const char* name = luaL_checkstring(L, 1);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->removeGlobalImageAsset(name);
    return 0;
}

static int Script_addGlobalFontAsset(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    const char* name = luaL_checkstring(L, 1);
    rive::FontHandle handle = (rive::FontHandle)luaL_checkinteger(L, 2);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->addGlobalFontAsset(name, handle);
    return 0;
}

static int Script_removeGlobalFontAsset(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    const char* name = luaL_checkstring(L, 1);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->removeGlobalFontAsset(name);
    return 0;
}

static int Script_addGlobalAudioAsset(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    const char* name = luaL_checkstring(L, 1);
    rive::AudioSourceHandle handle = (rive::AudioSourceHandle)luaL_checkinteger(L, 2);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->addGlobalAudioAsset(name, handle);
    return 0;
}

static int Script_removeGlobalAudioAsset(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    const char* name = luaL_checkstring(L, 1);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->removeGlobalAudioAsset(name);
    return 0;
}

// *****************************************************************************************

static int Script_requestViewModelNames(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);
    dmRiveCommands::GetCommandQueue()->requestViewModelNames(file);
    return 0;
}

static int Script_requestArtboardNames(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);
    dmRiveCommands::GetCommandQueue()->requestArtboardNames(file);
    return 0;
}

static int Script_requestViewModelEnums(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);
    dmRiveCommands::GetCommandQueue()->requestViewModelEnums(file);
    return 0;
}

static int Script_requestViewModelPropertyDefinitions(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);
    const char* viewmodel_name = luaL_checkstring(L, 2);
    dmRiveCommands::GetCommandQueue()->requestViewModelPropertyDefinitions(file, viewmodel_name);
    return 0;
}

static int Script_requestViewModelInstanceNames(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);
    const char* viewmodel_name = luaL_checkstring(L, 2);
    dmRiveCommands::GetCommandQueue()->requestViewModelInstanceNames(file, viewmodel_name);
    return 0;
}

static int Script_requestViewModelInstanceBool(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* viewmodel_name = luaL_checkstring(L, 2);
    dmRiveCommands::GetCommandQueue()->requestViewModelInstanceBool(handle, viewmodel_name);
    return 0;
}

static int Script_requestViewModelInstanceNumber(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* viewmodel_name = luaL_checkstring(L, 2);
    dmRiveCommands::GetCommandQueue()->requestViewModelInstanceNumber(handle, viewmodel_name);
    return 0;
}

static int Script_requestViewModelInstanceColor(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* viewmodel_name = luaL_checkstring(L, 2);
    dmRiveCommands::GetCommandQueue()->requestViewModelInstanceColor(handle, viewmodel_name);
    return 0;
}

static int Script_requestViewModelInstanceEnum(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* viewmodel_name = luaL_checkstring(L, 2);
    dmRiveCommands::GetCommandQueue()->requestViewModelInstanceEnum(handle, viewmodel_name);
    return 0;
}

static int Script_requestViewModelInstanceString(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* viewmodel_name = luaL_checkstring(L, 2);
    dmRiveCommands::GetCommandQueue()->requestViewModelInstanceString(handle, viewmodel_name);
    return 0;
}

static int Script_requestViewModelInstanceListSize(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* viewmodel_name = luaL_checkstring(L, 2);
    dmRiveCommands::GetCommandQueue()->requestViewModelInstanceListSize(handle, viewmodel_name);
    return 0;
}

static int Script_requestStateMachineNames(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ArtboardHandle artboard = (rive::ArtboardHandle)luaL_checkinteger(L, 1);
    dmRiveCommands::GetCommandQueue()->requestStateMachineNames(artboard);
    return 0;
}

static int Script_requestDefaultViewModelInfo(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ArtboardHandle artboard = (rive::ArtboardHandle)luaL_checkinteger(L, 1);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 2);
    dmRiveCommands::GetCommandQueue()->requestDefaultViewModelInfo(artboard, file);
    return 0;
}

// *****************************************************************************************

static const luaL_reg RIVE_COMMAND_FUNCTIONS[] =
{
    {"instantiateArtboardNamed",            Script_instantiateArtboardNamed},
    {"instantiateDefaultArtboard",          Script_instantiateDefaultArtboard},
    {"instantiateBlankViewModelInstance",   Script_instantiateBlankViewModelInstance},
    {"instantiateDefaultViewModelInstance", Script_instantiateDefaultViewModelInstance},
    {"instantiateViewModelInstanceNamed",   Script_instantiateViewModelInstanceNamed},
    {"referenceNestedViewModelInstance",    Script_referenceNestedViewModelInstance},
    {"referenceListViewModelInstance",      Script_referenceListViewModelInstance},

    {"setViewModelInstanceNestedViewModel",     Script_setViewModelInstanceNestedViewModel},
    {"insertViewModelInstanceListViewModel",    Script_insertViewModelInstanceListViewModel},
    {"appendViewModelInstanceListViewModel",    Script_appendViewModelInstanceListViewModel},
    {"swapViewModelInstanceListValues",         Script_swapViewModelInstanceListValues},

    {"removeViewModelInstanceListViewModelIndex",Script_removeViewModelInstanceListViewModelIndex},
    {"removeViewModelInstanceListViewModel",    Script_removeViewModelInstanceListViewModel},

    {"bindViewModelInstance",       Script_bindViewModelInstance},
    {"deleteViewModelInstance",     Script_deleteViewModelInstance},

    {"fireViewModelTrigger",        Script_fireViewModelTrigger},
    {"setViewModelInstanceBool",    Script_setViewModelInstanceBool},
    {"setViewModelInstanceNumber",  Script_setViewModelInstanceNumber},
    {"setViewModelInstanceColor",   Script_setViewModelInstanceColor},
    {"setViewModelInstanceEnum",    Script_setViewModelInstanceEnum},
    {"setViewModelInstanceString",  Script_setViewModelInstanceString},
    {"setViewModelInstanceImage",   Script_setViewModelInstanceImage},
    {"setViewModelInstanceArtboard",Script_setViewModelInstanceArtboard},

    {"subscribeToViewModelProperty",    Script_subscribeToViewModelProperty},
    {"unsubscribeToViewModelProperty",  Script_unsubscribeToViewModelProperty},

    {"loadFile",    Script_loadFile},
    {"deleteFile",  Script_deleteFile},
    {"decodeImage", Script_decodeImage},
    {"deleteImage", Script_deleteImage},
    {"decodeAudio", Script_decodeAudio},
    {"deleteAudio", Script_deleteAudio},
    {"decodeFont",  Script_decodeFont},
    {"deleteFont",  Script_deleteFont},

    {"addGlobalImageAsset",     Script_addGlobalImageAsset},
    {"removeGlobalImageAsset",  Script_removeGlobalImageAsset},
    {"addGlobalAudioAsset",     Script_addGlobalAudioAsset},
    {"removeGlobalAudioAsset",  Script_removeGlobalAudioAsset},
    {"addGlobalFontAsset",      Script_addGlobalFontAsset},
    {"removeGlobalFontAsset",   Script_removeGlobalFontAsset},

    {"requestViewModelNames",   Script_requestViewModelNames},
    {"requestArtboardNames",    Script_requestArtboardNames},
    {"requestViewModelEnums",   Script_requestViewModelEnums},

    {"requestViewModelPropertyDefinitions", Script_requestViewModelPropertyDefinitions},
    {"requestViewModelInstanceNames",       Script_requestViewModelInstanceNames},
    {"requestViewModelInstanceBool",        Script_requestViewModelInstanceBool},
    {"requestViewModelInstanceNumber",      Script_requestViewModelInstanceNumber},
    {"requestViewModelInstanceColor",       Script_requestViewModelInstanceColor},
    {"requestViewModelInstanceEnum",        Script_requestViewModelInstanceEnum},
    {"requestViewModelInstanceString",      Script_requestViewModelInstanceString},
    {"requestViewModelInstanceListSize",    Script_requestViewModelInstanceListSize},

    {"requestStateMachineNames",            Script_requestStateMachineNames},
    {"requestDefaultViewModelInfo",         Script_requestDefaultViewModelInfo},

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
