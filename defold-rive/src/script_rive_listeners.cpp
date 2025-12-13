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

#include "res_rive_data.h"
#include "script_rive_listeners.h"
#include <dmsdk/dlib/log.h>

namespace dmRive
{

static bool SetupCallback(dmScript::LuaCallbackInfo* callback, dmhash_t id, uint64_t requestId)
{
    // TODO: Make sure this is run on the Lua thread!

    if (!callback)
        return false;

    lua_State* L = dmScript::GetCallbackLuaContext(callback);

    if (!dmScript::SetupCallback(callback))
    {
        return false;
    }

    dmScript::PushHash(L, id); // the name of the callback function
    lua_newtable(L);

        // TODO: Do we want/need to pass the request id (i.e. exposigin it through all the script api's?)
        // lua_pushinteger(L, requestId);
        // lua_setfield(L, -2, "requestId");

    return true;
}

static void InvokeCallback(lua_State* L, dmScript::LuaCallbackInfo* callback)
{
    // TODO: Make sure this is run on the Lua thread!

    // We assume that the SetupCallback was successful in adding [self, messageName, table] on the stack
    dmScript::PCall(L, 3, 0); // self + # user arguments
    dmScript::TeardownCallback(callback);
}

static void PushStringArray(lua_State* L, const char* name, std::vector<std::string>& array)
{
    uint32_t size = array.size();

    // An empty table is already on the stack
    // Create a new one for the artboard names
    lua_createtable(L, size, 0);

    for (uint32_t i = 0; i < size; ++i)
    {
        lua_pushstring(L, array[i].c_str());
        lua_rawseti(L, -2, i+1);
    }

    lua_setfield(L, -2, name);
}

static void PushPropertiesArray(lua_State* L, const char* name, std::vector<rive::CommandQueue::FileListener::ViewModelPropertyData>& properties)
{
    uint32_t size = properties.size();

    // An empty table is already on the stack
    // Create a new one for the artboard names
    lua_createtable(L, size, 0);

    for (uint32_t i = 0; i < size; ++i)
    {
        lua_createtable(L, 0, 0);

            lua_pushstring(L, properties[i].name.c_str());
            lua_setfield(L, -2, "name");

            lua_pushstring(L, properties[i].metaData.c_str());
            lua_setfield(L, -2, "metaData");

            lua_pushinteger(L, (int)properties[i].type);
            lua_setfield(L, -2, "type");

        lua_rawseti(L, -2, i+1);
    }

    lua_setfield(L, -2, name);
}

static void PushEnumsArray(lua_State* L, const char* name, std::vector<rive::ViewModelEnum>& enums)
{
    uint32_t size = enums.size();

    // An empty table is already on the stack
    // Create a new one for the artboard names
    lua_createtable(L, size, 0);

    for (uint32_t i = 0; i < size; ++i)
    {
        lua_createtable(L, 0, 0);

            lua_pushstring(L, enums[i].name.c_str());
            lua_setfield(L, -2, "name");

            PushStringArray(L, "enumerants", enums[i].enumerants);

        lua_rawseti(L, -2, i+1);
    }

    lua_setfield(L, -2, name);
}

// ******************************************************************************************************************************

void FileListener::onArtboardsListed(const rive::FileHandle fileHandle, uint64_t requestId, std::vector<std::string> artboardNames)
{
    static dmhash_t id = dmHashString64("onArtboardsListed");

    if (m_Callback && SetupCallback(m_Callback, id, requestId))
    {
        lua_State* L = dmScript::GetCallbackLuaContext(m_Callback);
        PushStringArray(L, "artboardNames", artboardNames);
        InvokeCallback(L, m_Callback);
    }
}

void FileListener::onFileError(const rive::FileHandle, uint64_t requestId, std::string error)
{
    RiveSceneData* scene = (RiveSceneData*)requestId;
    if (scene)
    {
        dmLogError("%s: %s", dmHashReverseSafe64(scene->m_PathHash), error.c_str());
    }
    else
    {
        dmLogError("%s", error.c_str());
    }

    static dmhash_t id = dmHashString64("onFileError");
    if (m_Callback && SetupCallback(m_Callback, id, requestId))
    {
        lua_State* L = dmScript::GetCallbackLuaContext(m_Callback);

        lua_pushstring(L, error.c_str());
        lua_setfield(L, -2, "artboardNames");

        InvokeCallback(L, m_Callback);
    }
}

void FileListener::onFileDeleted(const rive::FileHandle file, uint64_t requestId)
{
    static dmhash_t id = dmHashString64("onFileDeleted");
    if (m_Callback && SetupCallback(m_Callback, id, requestId))
    {
        lua_State* L = dmScript::GetCallbackLuaContext(m_Callback);

        lua_pushinteger(L, (int)(uintptr_t)file);
        lua_setfield(L, -2, "file");

        InvokeCallback(L, m_Callback);
    }
}

void FileListener::onFileLoaded(const rive::FileHandle file, uint64_t requestId)
{
    static dmhash_t id = dmHashString64("onFileLoaded");
    if (m_Callback && SetupCallback(m_Callback, id, requestId))
    {
        lua_State* L = dmScript::GetCallbackLuaContext(m_Callback);

        lua_pushinteger(L, (int)(uintptr_t)file);
        lua_setfield(L, -2, "file");

        InvokeCallback(L, m_Callback);
    }
}

void FileListener::onViewModelsListed(const rive::FileHandle file, uint64_t requestId, std::vector<std::string> viewModelNames)
{
    static dmhash_t id = dmHashString64("onViewModelsListed");
    if (m_Callback && SetupCallback(m_Callback, id, requestId))
    {
        lua_State* L = dmScript::GetCallbackLuaContext(m_Callback);

        lua_pushinteger(L, (int)(uintptr_t)file);
        lua_setfield(L, -2, "file");
        PushStringArray(L, "viewModelNames", viewModelNames);

        InvokeCallback(L, m_Callback);
    }
}

void FileListener::onViewModelInstanceNamesListed(const rive::FileHandle file, uint64_t requestId, std::string viewModelName, std::vector<std::string> instanceNames)
{
    static dmhash_t id = dmHashString64("onViewModelInstanceNamesListed");
    if (m_Callback && SetupCallback(m_Callback, id, requestId))
    {
        lua_State* L = dmScript::GetCallbackLuaContext(m_Callback);

        lua_pushinteger(L, (int)(uintptr_t)file);
        lua_setfield(L, -2, "file");

        lua_pushstring(L, viewModelName.c_str());
        lua_setfield(L, -2, "viewModelName");

        PushStringArray(L, "instanceNames", instanceNames);

        InvokeCallback(L, m_Callback);
    }
}

void FileListener::onViewModelPropertiesListed(const rive::FileHandle file, uint64_t requestId, std::string viewModelName, std::vector<ViewModelPropertyData> properties)
{
    static dmhash_t id = dmHashString64("onViewModelPropertiesListed");
    if (m_Callback && SetupCallback(m_Callback, id, requestId))
    {
        lua_State* L = dmScript::GetCallbackLuaContext(m_Callback);

        lua_pushinteger(L, (int)(uintptr_t)file);
        lua_setfield(L, -2, "file");

        lua_pushstring(L, viewModelName.c_str());
        lua_setfield(L, -2, "viewModelName");

        PushPropertiesArray(L, "properties", properties);

        InvokeCallback(L, m_Callback);
    }
}

void FileListener::onViewModelEnumsListed(const rive::FileHandle file, uint64_t requestId, std::vector<rive::ViewModelEnum> enums)
{
    static dmhash_t id = dmHashString64("onViewModelEnumsListed");
    if (m_Callback && SetupCallback(m_Callback, id, requestId))
    {
        lua_State* L = dmScript::GetCallbackLuaContext(m_Callback);

        lua_pushinteger(L, (int)(uintptr_t)file);
        lua_setfield(L, -2, "file");

        PushEnumsArray(L, "enums", enums);

        InvokeCallback(L, m_Callback);
    }
}

// ******************************************************************************************************************************

void RenderImageListener::onRenderImageDecoded(const rive::RenderImageHandle, uint64_t requestId)
{

}
void RenderImageListener::onRenderImageError(const rive::RenderImageHandle, uint64_t requestId, std::string error)
{

}
void RenderImageListener::onRenderImageDeleted(const rive::RenderImageHandle, uint64_t requestId)
{

}

// ******************************************************************************************************************************

void AudioSourceListener::onAudioSourceDecoded(const rive::AudioSourceHandle, uint64_t requestId)
{

}
void AudioSourceListener::onAudioSourceError(const rive::AudioSourceHandle, uint64_t requestId, std::string error)
{

}
void AudioSourceListener::onAudioSourceDeleted(const rive::AudioSourceHandle, uint64_t requestId)
{

}

// ******************************************************************************************************************************


void FontListener::onFontDecoded(const rive::FontHandle, uint64_t requestId)
{

}
void FontListener::onFontError(const rive::FontHandle, uint64_t requestId, std::string error)
{

}
void FontListener::onFontDeleted(const rive::FontHandle, uint64_t requestId)
{

}

// ******************************************************************************************************************************

void ArtboardListener::onArtboardError(const rive::ArtboardHandle, uint64_t requestId, std::string error)
{

}
void ArtboardListener::onDefaultViewModelInfoReceived(const rive::ArtboardHandle, uint64_t requestId, std::string viewModelName, std::string instanceName)
{

}
void ArtboardListener::onArtboardDeleted(const rive::ArtboardHandle, uint64_t requestId)
{

}
void ArtboardListener::onStateMachinesListed(const rive::ArtboardHandle, uint64_t requestId, std::vector<std::string> stateMachineNames)
{

}

// ******************************************************************************************************************************

void ViewModelInstanceListener::onViewModelInstanceError(const rive::ViewModelInstanceHandle, uint64_t requestId, std::string error)
{

}

void ViewModelInstanceListener::onViewModelDeleted(const rive::ViewModelInstanceHandle, uint64_t requestId)
{

}

void ViewModelInstanceListener::onViewModelDataReceived(const rive::ViewModelInstanceHandle, uint64_t requestId, rive::CommandQueue::ViewModelInstanceData)
{

}

void ViewModelInstanceListener::onViewModelListSizeReceived(const rive::ViewModelInstanceHandle, uint64_t requestId, std::string path, size_t size)
{

}

// ******************************************************************************************************************************

void StateMachineListener::onStateMachineError(const rive::StateMachineHandle, uint64_t requestId, std::string error)
{

}
void StateMachineListener::onStateMachineDeleted(const rive::StateMachineHandle, uint64_t requestId)
{

}
void StateMachineListener::onStateMachineSettled(const rive::StateMachineHandle, uint64_t requestId)
{

}

} // namespace dmRive

#endif // DM_RIVE_UNSUPPORTED
