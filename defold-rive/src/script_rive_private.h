// Copyright 2020 The Defold Foundation
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

#ifndef DM_GAMESYS_SCRIPT_RIVE_PRIVATE_H
#define DM_GAMESYS_SCRIPT_RIVE_PRIVATE_H

#include <dmsdk/script/script.h>
#include <dmsdk/resource/resource.h>

#include <rive/artboard.hpp>
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

namespace rive
{
    class ArtboardInstance;
    class ViewModelInstanceRuntime;
    class ViewModelInstanceValueRuntime;
}

namespace dmRive
{
    // struct RiveFile
    // {
    //     RiveSceneData* m_Resource;
    // };

    // struct Artboard
    // {
    //     RiveSceneData*          m_Resource; // The original file that it belongs to
    //     rive::ArtboardInstance* m_Instance; // The instance
    // };

    // struct ViewModel
    // {
    //     RiveSceneData*                  m_Resource; // The original file that it belongs to
    //     rive::ViewModelInstanceRuntime* m_Instance; // The instance
    // };

    // struct ViewModelProperty
    // {
    //     RiveSceneData*                          m_Resource; // The original file that it belongs to
    //     rive::ViewModelInstanceValueRuntime*    m_Instance; // The instance
    // };

    // bool        IsRiveFile(lua_State* L, int index);
    // void        PushRiveFile(lua_State* L, RiveSceneData* resource);
    // RiveFile*   ToRiveFile(lua_State* L, int index);
    // RiveFile*   CheckRiveFile(lua_State* L, int index);

    // void        PushArtboard(lua_State* L, RiveSceneData* resource, rive::ArtboardInstance* instance);
    // Artboard*   CheckArtboard(lua_State* L, int index);

    // void        PushViewModel(lua_State* L, RiveSceneData* resource, rive::ViewModelInstanceRuntime* instance);
    // ViewModel*  CheckViewModel(lua_State* L, int index);

    // void                PushViewModelProperty(lua_State* L, RiveSceneData* resource, rive::ViewModelInstanceValueRuntime* instance);
    // ViewModelProperty*  CheckViewModelProperty(lua_State* L, int index);

    void ScriptInitializeFile(lua_State* L, dmResource::HFactory factory);
    void ScriptInitializeArtboard(lua_State* L, dmResource::HFactory factory);
    void ScriptInitializeViewModel(lua_State* L, dmResource::HFactory factory);
    void ScriptInitializeViewModelProperty(lua_State* L, dmResource::HFactory factory);

    // Deprecated
    void ScriptInitializeDataBinding(lua_State* L, dmResource::HFactory factory);
}

#endif // DM_GAMESYS_SCRIPT_RIVE_PRIVATE_H
