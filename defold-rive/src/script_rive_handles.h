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

#ifndef DM_GAMESYS_SCRIPT_RIVE_HANDLES_H
#define DM_GAMESYS_SCRIPT_RIVE_HANDLES_H

#include <dmsdk/script/script.h>

#include <defold/rive.h>

namespace dmRive
{
    void                          RegisterScriptRiveHandles(lua_State* L);

    void                          PushFileHandle(lua_State* L, rive::FileHandle handle);
    rive::FileHandle              CheckFileHandle(lua_State* L, int index);

    void                          PushArtboardHandle(lua_State* L, rive::ArtboardHandle handle);
    rive::ArtboardHandle          CheckArtboardHandle(lua_State* L, int index);

    void                          PushStateMachineHandle(lua_State* L, rive::StateMachineHandle handle);
    rive::StateMachineHandle      CheckStateMachineHandle(lua_State* L, int index);

    void                          PushViewModelInstanceHandle(lua_State* L, rive::ViewModelInstanceHandle handle);
    rive::ViewModelInstanceHandle CheckViewModelInstanceHandle(lua_State* L, int index);

    void                          PushRenderImageHandle(lua_State* L, rive::RenderImageHandle handle);
    rive::RenderImageHandle       CheckRenderImageHandle(lua_State* L, int index);

    void                          PushAudioSourceHandle(lua_State* L, rive::AudioSourceHandle handle);
    rive::AudioSourceHandle       CheckAudioSourceHandle(lua_State* L, int index);

    void                          PushFontHandle(lua_State* L, rive::FontHandle handle);
    rive::FontHandle              CheckFontHandle(lua_State* L, int index);
} // namespace dmRive

#endif // DM_GAMESYS_SCRIPT_RIVE_HANDLES_H
