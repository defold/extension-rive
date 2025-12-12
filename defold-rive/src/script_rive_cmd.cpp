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

/**
 * Returns the artboard handle created for the named view model.
 * @name cmd.instantiateArtboardNamed(file_handle, viewmodel_name)
 * @param file_handle [type: FileHandle] Handle to a previously loaded Rive file.
 * @param viewmodel_name [type: string] Name of the view model to instantiate.
 * @return artboard_handle [type: ArtboardHandle] Artboard handle created for the named view model.
 */
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

/**
 * Returns the default artboard handle for the file.
 * @name cmd.instantiateDefaultArtboard(file_handle)
 * @param file_handle [type: FileHandle] Handle to a previously loaded Rive file.
 * @return artboard_handle [type: ArtboardHandle] Default artboard handle for the file.
 */
static int Script_instantiateDefaultArtboard(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    rive::ArtboardHandle handle = queue->instantiateDefaultArtboard(file);

    lua_pushinteger(L, (lua_Integer)handle);
    return 1;
}

/**
 * Returns a blank view model instance handle for the given artboard or view model.
 * @name cmd.instantiateBlankViewModelInstance(file_handle, artboard_or_viewmodel)
 * @param file_handle [type: FileHandle] Handle to a previously loaded Rive file.
 * @param artboard_or_viewmodel [type: ArtboardHandle|string] Artboard handle or view model name that identifies where to instantiate.
 * @return view_model_instance_handle [type: ViewModelInstanceHandle] Blank view model instance handle.
 */
static int Script_instantiateBlankViewModelInstance(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);

    if (lua_isnil(L, 2) || lua_isnone(L, 2))
    {
        return luaL_error(L, "Argument #2 must be an integer (artboard handle) or a string (view model name)");
    }

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

/**
 * Returns a default-populated view model instance handle for the given artboard or view model.
 * @name cmd.instantiateDefaultViewModelInstance(file_handle, artboard_or_viewmodel)
 * @param file_handle [type: FileHandle] Handle to a previously loaded Rive file.
 * @param artboard_or_viewmodel [type: ArtboardHandle|string] Artboard handle or view model name the instance should derive from.
 * @return view_model_instance_handle [type: ViewModelInstanceHandle] Default view model instance handle.
 */
static int Script_instantiateDefaultViewModelInstance(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);

    if (lua_isnil(L, 2) || lua_isnone(L, 2))
    {
        return luaL_error(L, "Argument #2 must be an integer (artboard handle) or a string (view model name)");
    }

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

/**
 * Creates a named view model instance and returns its handle.
 * @name cmd.instantiateViewModelInstanceNamed(file_handle, artboard_or_viewmodel, instance_name)
 * @param file_handle [type: FileHandle] Handle to a previously loaded Rive file.
 * @param artboard_or_viewmodel [type: ArtboardHandle|string] Artboard handle or view model name that will host the instance.
 * @param instance_name [type: string] Name to assign to the new view model instance.
 * @return view_model_instance_handle [type: ViewModelInstanceHandle] Named view model instance handle.
 */
static int Script_instantiateViewModelInstanceNamed(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);

    if (lua_isnil(L, 2) || lua_isnone(L, 2))
    {
        return luaL_error(L, "Argument #2 must be an integer (artboard handle) or a string (view model name)");
    }

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

/**
 * Returns a handle to the nested view model at the given path.
 * @name cmd.referenceNestedViewModelInstance(view_model_handle, path)
 * @param view_model_handle [type: ViewModelInstanceHandle] Parent view model instance handle.
 * @param path [type: string] Dot-delimited path to the nested view model.
 * @return view_model_handle [type: ViewModelInstanceHandle] Handle to the nested view model.
 */
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

/**
 * Returns the handle for the list entry at the specified path and index.
 * @name cmd.referenceListViewModelInstance(view_model_handle, path, index)
 * @param view_model_handle [type: ViewModelInstanceHandle] Parent view model instance handle.
 * @param path [type: string] Dot-delimited path to the list view model.
 * @param index [type: number] Index within the list entry to reference.
 * @return view_model_handle [type: ViewModelInstanceHandle] Handle to the referenced list entry.
 */
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

/**
 * Replaces the nested view model at the given path with the supplied handle.
 * @name cmd.setViewModelInstanceNestedViewModel(view_model_handle, path, nested_handle)
 * @param view_model_handle [type: ViewModelInstanceHandle] View model instance whose nested child is updated.
 * @param path [type: string] Path to the nested view model.
 * @param nested_handle [type: ViewModelInstanceHandle] Handle of the nested view model to attach.
 */
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

/**
 * Inserts a nested view model into the list at the given index.
 * @name cmd.insertViewModelInstanceListViewModel(view_model_handle, path, nested_handle, index)
 * @param view_model_handle [type: ViewModelInstanceHandle] View model instance owning the list.
 * @param path [type: string] Path to the target list.
 * @param nested_handle [type: ViewModelInstanceHandle] Handle of the view model to insert.
 * @param index [type: number] Destination index for the insertion.
 */
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

/**
 * Appends a nested view model to the list at the specified path.
 * @name cmd.appendViewModelInstanceListViewModel(view_model_handle, path, nested_handle)
 * @param view_model_handle [type: ViewModelInstanceHandle] View model instance owning the list.
 * @param path [type: string] Path to the target list.
 * @param nested_handle [type: ViewModelInstanceHandle] Handle of the view model to append.
 */
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

/**
 * Removes the entry at the supplied index from the nested list.
 * @name cmd.removeViewModelInstanceListViewModelIndex(view_model_handle, path, index)
 * @param view_model_handle [type: ViewModelInstanceHandle] View model instance owning the list.
 * @param path [type: string] Path to the target list.
 * @param index [type: number] Index of the entry to remove.
 */
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

/**
 * Removes the specified nested view model from the list at the path.
 * @name cmd.removeViewModelInstanceListViewModel(view_model_handle, path, nested_handle)
 * @param view_model_handle [type: ViewModelInstanceHandle] View model instance owning the list.
 * @param path [type: string] Path to the target list.
 * @param nested_handle [type: ViewModelInstanceHandle] Handle of the view model to remove.
 */
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

/**
 * Deletes the view model instance handle.
 * @name cmd.deleteViewModelInstance(view_model_handle)
 * @param view_model_handle [type: ViewModelInstanceHandle] View model instance to delete.
 */
static int Script_deleteViewModelInstance(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->deleteViewModelInstance(handle);
    return 0;
}

/**
 * Swaps two entries in the nested list.
 * @name cmd.swapViewModelInstanceListValues(view_model_handle, path, indexa, indexb)
 * @param view_model_handle [type: ViewModelInstanceHandle] View model instance that owns the list.
 * @param path [type: string] Path to the nested list.
 * @param indexa [type: number] First index to swap.
 * @param indexb [type: number] Second index to swap.
 */
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


/**
 * Binds the state machine to the provided view model instance.
 * @name cmd.bindViewModelInstance(state_machine_handle, view_model_handle)
 * @param state_machine_handle [type: StateMachineHandle] State machine handle.
 * @param view_model_handle [type: ViewModelInstanceHandle] View model instance handle.
 */
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


/**
 * Fires a trigger on the view model instance.
 * @name cmd.fireViewModelTrigger(view_model_handle, trigger_path)
 * @param view_model_handle [type: ViewModelInstanceHandle] View model instance handle.
 * @param trigger_path [type: string] Trigger path to activate.
 */
static int Script_fireViewModelTrigger(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* path = luaL_checkstring(L, 2);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->fireViewModelTrigger(handle, path);
    return 0;
}

/**
 * Updates the boolean property at the path.
 * @name cmd.setViewModelInstanceBool(view_model_handle, path, value)
 * @param view_model_handle [type: ViewModelInstanceHandle] View model instance handle.
 * @param path [type: string] Path to the boolean property.
 * @param value [type: boolean] New boolean value.
 */
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

/**
 * Updates the numeric property at the path.
 * @name cmd.setViewModelInstanceNumber(view_model_handle, path, value)
 * @param view_model_handle [type: ViewModelInstanceHandle] View model instance handle.
 * @param path [type: string] Path to the numeric property.
 * @param value [type: number] New numeric value.
 */
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

/**
 * Updates the color property using the supplied vector.
 * @name cmd.setViewModelInstanceColor(view_model_handle, path, vector4_color)
 * @param view_model_handle [type: ViewModelInstanceHandle] View model instance handle.
 * @param path [type: string] Path to the color property.
 * @param vector4_color [type: vector4] Color encoded as a Defold Vector4 (WXYZ).
 */
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

/**
 * Updates the enum property at the path.
 * @name cmd.setViewModelInstanceEnum(view_model_handle, path, enum_string)
 * @param view_model_handle [type: ViewModelInstanceHandle] View model instance handle.
 * @param path [type: string] Path to the enum property.
 * @param enum_string [type: string] Enum name to select.
 */
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

/**
 * Updates the string property at the path.
 * @name cmd.setViewModelInstanceString(view_model_handle, path, string_value)
 * @param view_model_handle [type: ViewModelInstanceHandle] View model instance handle.
 * @param path [type: string] Path to the string property.
 * @param string_value [type: string] New string value.
 */
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

/**
 * Updates the image property at the path.
 * @name cmd.setViewModelInstanceImage(view_model_handle, path, render_image_handle)
 * @param view_model_handle [type: ViewModelInstanceHandle] View model instance handle.
 * @param path [type: string] Path to the image property.
 * @param render_image_handle [type: RenderImageHandle] Render image handle to assign.
 */
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

/**
 * Updates the artboard reference at the path.
 * @name cmd.setViewModelInstanceArtboard(view_model_handle, path, artboard_handle)
 * @param view_model_handle [type: ViewModelInstanceHandle] View model instance handle.
 * @param path [type: string] Path to the artboard reference.
 * @param artboard_handle [type: ArtboardHandle] Artboard handle to assign.
 */
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

/**
 * Subscribes for updates to the named property.
 * @name cmd.subscribeToViewModelProperty(view_model_handle, path, data_type)
 * @param view_model_handle [type: ViewModelInstanceHandle] View model instance handle.
 * @param path [type: string] Path to the property to subscribe.
 * @param data_type [type: enum] Data type identifier from rive::DataType.
 */
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

/**
 * Cancels a previous subscription.
 * @name cmd.unsubscribeToViewModelProperty(view_model_handle, path, data_type)
 * @param view_model_handle [type: ViewModelInstanceHandle] View model instance handle.
 * @param path [type: string] Path to the property.
 * @param data_type [type: enum] Data type identifier previously subscribed.
 */
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

/**
 * Loads Rive bytes and returns a file handle.
 * @name cmd.loadFile(riv_bytes)
 * @param riv_bytes [type: string] Raw Rive file data.
 * @return file_handle [type: FileHandle] Loaded Rive file handle.
 */
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

/**
 * Deletes the runtime file handle.
 * @name cmd.deleteFile(file_handle)
 * @param file_handle [type: FileHandle] File handle to delete.
 */
static int Script_deleteFile(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::FileHandle handle = (rive::FileHandle)luaL_checkinteger(L, 1);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->deleteFile(handle);
    return 0;
}

/**
 * Decodes image bytes and returns a render image handle.
 * @name cmd.decodeImage(image_bytes)
 * @param image_bytes [type: string] Encoded image data.
 * @return render_image_handle [type: RenderImageHandle] Decoded render image handle.
 */
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

/**
 * Deletes the render image handle.
 * @name cmd.deleteImage(image_handle)
 * @param image_handle [type: RenderImageHandle] Render image handle to delete.
 */
static int Script_deleteImage(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::RenderImageHandle handle = (rive::RenderImageHandle)luaL_checkinteger(L, 1);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->deleteImage(handle);
    return 0;
}

/**
 * Decodes audio bytes and returns an audio source handle.
 * @name cmd.decodeAudio(audio_bytes)
 * @param audio_bytes [type: string] Encoded audio data.
 * @return audio_handle [type: AudioSourceHandle] Decoded audio source handle.
 */
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

/**
 * Deletes the audio source handle.
 * @name cmd.deleteAudio(audio_handle)
 * @param audio_handle [type: AudioSourceHandle] Audio source handle to delete.
 */
static int Script_deleteAudio(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::AudioSourceHandle handle = (rive::AudioSourceHandle)luaL_checkinteger(L, 1);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->deleteAudio(handle);
    return 0;
}

/**
 * Decodes font bytes and returns a font handle.
 * @name cmd.decodeFont(font_bytes)
 * @param font_bytes [type: string] Encoded font data.
 * @return font_handle [type: FontHandle] Decoded font handle.
 */
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

/**
 * Deletes the font handle.
 * @name cmd.deleteFont(font_handle)
 * @param font_handle [type: FontHandle] Font handle to delete.
 */
static int Script_deleteFont(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::FontHandle handle = (rive::FontHandle)luaL_checkinteger(L, 1);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->deleteFont(handle);
    return 0;
}

/**
 * Registers a global image asset.
 * @name cmd.addGlobalImageAsset(asset_name, render_image_handle)
 * @param asset_name [type: string] Name used to reference the global asset.
 * @param render_image_handle [type: RenderImageHandle] Render image handle to register.
 */
static int Script_addGlobalImageAsset(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    const char* name = luaL_checkstring(L, 1);
    rive::RenderImageHandle handle = (rive::RenderImageHandle)luaL_checkinteger(L, 2);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->addGlobalImageAsset(name, handle);
    return 0;
}

/**
 * Unregisters the named global image asset.
 * @name cmd.removeGlobalImageAsset(asset_name)
 * @param asset_name [type: string] Name of the asset to remove.
 */
static int Script_removeGlobalImageAsset(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    const char* name = luaL_checkstring(L, 1);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->removeGlobalImageAsset(name);
    return 0;
}

/**
 * Registers a global font asset.
 * @name cmd.addGlobalFontAsset(asset_name, font_handle)
 * @param asset_name [type: string] Name used to reference the global font.
 * @param font_handle [type: FontHandle] Font handle to register.
 */
static int Script_addGlobalFontAsset(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    const char* name = luaL_checkstring(L, 1);
    rive::FontHandle handle = (rive::FontHandle)luaL_checkinteger(L, 2);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->addGlobalFontAsset(name, handle);
    return 0;
}

/**
 * Unregisters the named global font asset.
 * @name cmd.removeGlobalFontAsset(asset_name)
 * @param asset_name [type: string] Name of the font asset to remove.
 */
static int Script_removeGlobalFontAsset(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    const char* name = luaL_checkstring(L, 1);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->removeGlobalFontAsset(name);
    return 0;
}

/**
 * Registers a global audio asset.
 * @name cmd.addGlobalAudioAsset(asset_name, audio_handle)
 * @param asset_name [type: string] Name used to reference the global audio.
 * @param audio_handle [type: AudioSourceHandle] Audio source handle to register.
 */
static int Script_addGlobalAudioAsset(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    const char* name = luaL_checkstring(L, 1);
    rive::AudioSourceHandle handle = (rive::AudioSourceHandle)luaL_checkinteger(L, 2);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->addGlobalAudioAsset(name, handle);
    return 0;
}

/**
 * Unregisters the named global audio asset.
 * @name cmd.removeGlobalAudioAsset(asset_name)
 * @param asset_name [type: string] Name of the audio asset to remove.
 */
static int Script_removeGlobalAudioAsset(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    const char* name = luaL_checkstring(L, 1);
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    queue->removeGlobalAudioAsset(name);
    return 0;
}

// *****************************************************************************************

/**
 * Requests view model names for the file.
 * @name cmd.requestViewModelNames(file_handle)
 * @param file_handle [type: FileHandle] File handle whose view models to query.
 */
static int Script_requestViewModelNames(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);
    dmRiveCommands::GetCommandQueue()->requestViewModelNames(file);
    return 0;
}

/**
 * Requests artboard names for the file.
 * @name cmd.requestArtboardNames(file_handle)
 * @param file_handle [type: FileHandle] File handle whose artboards to query.
 */
static int Script_requestArtboardNames(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);
    dmRiveCommands::GetCommandQueue()->requestArtboardNames(file);
    return 0;
}

/**
 * Requests enum definitions for the file.
 * @name cmd.requestViewModelEnums(file_handle)
 * @param file_handle [type: FileHandle] File handle whose enums to query.
 */
static int Script_requestViewModelEnums(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);
    dmRiveCommands::GetCommandQueue()->requestViewModelEnums(file);
    return 0;
}

/**
 * Requests property definitions for the view model.
 * @name cmd.requestViewModelPropertyDefinitions(file_handle, viewmodel_name)
 * @param file_handle [type: FileHandle] File handle whose view models to query.
 * @param viewmodel_name [type: string] View model name whose property metadata to request.
 */
static int Script_requestViewModelPropertyDefinitions(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);
    const char* viewmodel_name = luaL_checkstring(L, 2);
    dmRiveCommands::GetCommandQueue()->requestViewModelPropertyDefinitions(file, viewmodel_name);
    return 0;
}

/**
 * Requests instance names for the view model.
 * @name cmd.requestViewModelInstanceNames(file_handle, viewmodel_name)
 * @param file_handle [type: FileHandle] File handle whose view models to query.
 * @param viewmodel_name [type: string] View model name to inspect.
 */
static int Script_requestViewModelInstanceNames(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::FileHandle file = (rive::FileHandle)luaL_checkinteger(L, 1);
    const char* viewmodel_name = luaL_checkstring(L, 2);
    dmRiveCommands::GetCommandQueue()->requestViewModelInstanceNames(file, viewmodel_name);
    return 0;
}

/**
 * Requests the boolean value for the property.
 * @name cmd.requestViewModelInstanceBool(instance_handle, property_name)
 * @param instance_handle [type: ViewModelInstanceHandle] View model instance handle.
 * @param property_name [type: string] Name of the bool property.
 */
static int Script_requestViewModelInstanceBool(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* viewmodel_name = luaL_checkstring(L, 2);
    dmRiveCommands::GetCommandQueue()->requestViewModelInstanceBool(handle, viewmodel_name);
    return 0;
}

/**
 * Requests the numeric value for the property.
 * @name cmd.requestViewModelInstanceNumber(instance_handle, property_name)
 * @param instance_handle [type: ViewModelInstanceHandle] View model instance handle.
 * @param property_name [type: string] Name of the numeric property.
 */
static int Script_requestViewModelInstanceNumber(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* viewmodel_name = luaL_checkstring(L, 2);
    dmRiveCommands::GetCommandQueue()->requestViewModelInstanceNumber(handle, viewmodel_name);
    return 0;
}

/**
 * Requests the color value for the property.
 * @name cmd.requestViewModelInstanceColor(instance_handle, property_name)
 * @param instance_handle [type: ViewModelInstanceHandle] View model instance handle.
 * @param property_name [type: string] Name of the color property.
 */
static int Script_requestViewModelInstanceColor(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* viewmodel_name = luaL_checkstring(L, 2);
    dmRiveCommands::GetCommandQueue()->requestViewModelInstanceColor(handle, viewmodel_name);
    return 0;
}

/**
 * Requests the enum value for the property.
 * @name cmd.requestViewModelInstanceEnum(instance_handle, property_name)
 * @param instance_handle [type: ViewModelInstanceHandle] View model instance handle.
 * @param property_name [type: string] Name of the enum property.
 */
static int Script_requestViewModelInstanceEnum(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* viewmodel_name = luaL_checkstring(L, 2);
    dmRiveCommands::GetCommandQueue()->requestViewModelInstanceEnum(handle, viewmodel_name);
    return 0;
}

/**
 * Requests the string value for the property.
 * @name cmd.requestViewModelInstanceString(instance_handle, property_name)
 * @param instance_handle [type: ViewModelInstanceHandle] View model instance handle.
 * @param property_name [type: string] Name of the string property.
 */
static int Script_requestViewModelInstanceString(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* viewmodel_name = luaL_checkstring(L, 2);
    dmRiveCommands::GetCommandQueue()->requestViewModelInstanceString(handle, viewmodel_name);
    return 0;
}

/**
 * Requests the list size for the property.
 * @name cmd.requestViewModelInstanceListSize(instance_handle, property_name)
 * @param instance_handle [type: ViewModelInstanceHandle] View model instance handle.
 * @param property_name [type: string] Name of the list property.
 */
static int Script_requestViewModelInstanceListSize(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ViewModelInstanceHandle handle = (rive::ViewModelInstanceHandle)luaL_checkinteger(L, 1);
    const char* viewmodel_name = luaL_checkstring(L, 2);
    dmRiveCommands::GetCommandQueue()->requestViewModelInstanceListSize(handle, viewmodel_name);
    return 0;
}

/**
 * Requests state machine names for the artboard.
 * @name cmd.requestStateMachineNames(artboard_handle)
 * @param artboard_handle [type: ArtboardHandle] Artboard handle to query.
 */
static int Script_requestStateMachineNames(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    rive::ArtboardHandle artboard = (rive::ArtboardHandle)luaL_checkinteger(L, 1);
    dmRiveCommands::GetCommandQueue()->requestStateMachineNames(artboard);
    return 0;
}

/**
 * Requests metadata about the default view model or artboard.
 * @name cmd.requestDefaultViewModelInfo(artboard_handle, file_handle)
 * @param artboard_handle [type: ArtboardHandle] Artboard handle to query.
 * @param file_handle [type: FileHandle] File handle providing metadata.
 */
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
