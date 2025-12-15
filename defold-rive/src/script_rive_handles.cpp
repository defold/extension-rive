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

#include "script_defold.h"
#include "script_rive_handles.h"

#include <stdint.h>

namespace dmRive
{
    struct HandleUserData
    {
        uint64_t m_Handle;
    };

    template <typename HandleT>
    struct LuaHandle
    {
        static const char* s_TypeName;

        static int LuaGC(lua_State* L)
        {
            (void)Check(L, 1);
            return 0;
        }

        static int LuaToString(lua_State* L)
        {
            HandleT  handle = Check(L, 1);
            uint64_t value = (uint64_t)(uintptr_t)handle;
            lua_pushfstring(L, "%s(%p)", s_TypeName, (void*)(uintptr_t)value);
            return 1;
        }

        static void Register(lua_State* L, const char* name)
        {
            s_TypeName = name;
            static const luaL_reg meta[] = {
                { "__gc", LuaGC },
                { "__tostring", LuaToString },
                { 0, 0 }
            };
            RegisterUserType(L, name, name, 0, meta);
        }

        static void Push(lua_State* L, HandleT handle)
        {
            HandleUserData* data = (HandleUserData*)lua_newuserdata(L, sizeof(HandleUserData));
            data->m_Handle = (uintptr_t)handle;
            luaL_getmetatable(L, s_TypeName);
            lua_setmetatable(L, -2);
        }

        static HandleT Check(lua_State* L, int index)
        {
            HandleUserData* data = (HandleUserData*)CheckUserType(L, index, s_TypeName);
            return (HandleT)data->m_Handle;
        }
    };

    template <typename HandleT>
    const char* LuaHandle<HandleT>::s_TypeName = nullptr;

    void RegisterScriptRiveHandles(lua_State* L)
    {
        LuaHandle<rive::FileHandle>::Register(L, "rive.FileHandle");
        LuaHandle<rive::ArtboardHandle>::Register(L, "rive.ArtboardHandle");
        LuaHandle<rive::StateMachineHandle>::Register(L, "rive.StateMachineHandle");
        LuaHandle<rive::ViewModelInstanceHandle>::Register(L, "rive.ViewModelInstanceHandle");
        LuaHandle<rive::RenderImageHandle>::Register(L, "rive.RenderImageHandle");
        LuaHandle<rive::AudioSourceHandle>::Register(L, "rive.AudioSourceHandle");
        LuaHandle<rive::FontHandle>::Register(L, "rive.FontHandle");
    }

    void PushFileHandle(lua_State* L, rive::FileHandle handle)
    {
        LuaHandle<rive::FileHandle>::Push(L, handle);
    }
    rive::FileHandle CheckFileHandle(lua_State* L, int index)
    {
        return LuaHandle<rive::FileHandle>::Check(L, index);
    }

    void PushArtboardHandle(lua_State* L, rive::ArtboardHandle handle)
    {
        LuaHandle<rive::ArtboardHandle>::Push(L, handle);
    }
    rive::ArtboardHandle CheckArtboardHandle(lua_State* L, int index)
    {
        return LuaHandle<rive::ArtboardHandle>::Check(L, index);
    }

    void PushStateMachineHandle(lua_State* L, rive::StateMachineHandle handle)
    {
        LuaHandle<rive::StateMachineHandle>::Push(L, handle);
    }
    rive::StateMachineHandle CheckStateMachineHandle(lua_State* L, int index)
    {
        return LuaHandle<rive::StateMachineHandle>::Check(L, index);
    }

    void PushViewModelInstanceHandle(lua_State* L, rive::ViewModelInstanceHandle handle)
    {
        LuaHandle<rive::ViewModelInstanceHandle>::Push(L, handle);
    }
    rive::ViewModelInstanceHandle CheckViewModelInstanceHandle(lua_State* L, int index)
    {
        return LuaHandle<rive::ViewModelInstanceHandle>::Check(L, index);
    }

    void PushRenderImageHandle(lua_State* L, rive::RenderImageHandle handle)
    {
        LuaHandle<rive::RenderImageHandle>::Push(L, handle);
    }
    rive::RenderImageHandle CheckRenderImageHandle(lua_State* L, int index)
    {
        return LuaHandle<rive::RenderImageHandle>::Check(L, index);
    }

    void PushAudioSourceHandle(lua_State* L, rive::AudioSourceHandle handle)
    {
        LuaHandle<rive::AudioSourceHandle>::Push(L, handle);
    }
    rive::AudioSourceHandle CheckAudioSourceHandle(lua_State* L, int index)
    {
        return LuaHandle<rive::AudioSourceHandle>::Check(L, index);
    }

    void PushFontHandle(lua_State* L, rive::FontHandle handle)
    {
        LuaHandle<rive::FontHandle>::Push(L, handle);
    }
    rive::FontHandle CheckFontHandle(lua_State* L, int index)
    {
        return LuaHandle<rive::FontHandle>::Check(L, index);
    }
} // namespace dmRive
