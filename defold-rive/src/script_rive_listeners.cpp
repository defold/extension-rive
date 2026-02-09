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
#include "viewmodel_instance_registry.h"
#include <assert.h>
#include <common/commands.h>
#include <dmsdk/dlib/log.h>

namespace dmRive
{

template <typename T>
static void EnsureTableCapacity(dmHashTable<dmhash_t, T*>& table, uint32_t capacity)
{
    if (table.Capacity() >= capacity)
    {
        return;
    }
    uint32_t grow = capacity - table.Capacity();
    table.OffsetCapacity((int32_t)grow);
}

struct DeleteContext
{
};

static void DeletePropertyValue(DeleteContext*, const dmhash_t*, rive::CommandQueue::ViewModelInstanceData** value)
{
    if (value && *value)
    {
        delete *value;
        *value = 0;
    }
}

static void DeleteListSize(DeleteContext*, const dmhash_t*, size_t** value)
{
    if (value && *value)
    {
        delete *value;
        *value = 0;
    }
}

struct ViewModelPropertyRequest
{
    rive::ViewModelInstanceHandle m_Instance;
    rive::FileHandle m_File;
};

static dmMutex::HMutex g_ViewModelPropertyRequestMutex = 0;
static dmHashTable<uint64_t, ViewModelPropertyRequest*> g_PendingDefaultViewModelRequests;
static dmHashTable<uint64_t, ViewModelPropertyRequest*> g_PendingViewModelPropertyRequests;

static void EnsureViewModelPropertyRequestCapacity(dmHashTable<uint64_t, ViewModelPropertyRequest*>& table, uint32_t capacity)
{
    if (!g_ViewModelPropertyRequestMutex)
    {
        g_ViewModelPropertyRequestMutex = dmMutex::New();
    }
    if (table.Capacity() >= capacity)
    {
        return;
    }
    uint32_t grow = capacity - table.Capacity();
    table.OffsetCapacity((int32_t)grow);
}

static uint64_t RegisterViewModelPropertyRequest(dmHashTable<uint64_t, ViewModelPropertyRequest*>& table, ViewModelPropertyRequest* request)
{
    EnsureViewModelPropertyRequestCapacity(table, 32);
    DM_MUTEX_SCOPED_LOCK(g_ViewModelPropertyRequestMutex);
    if (table.Full())
    {
        EnsureViewModelPropertyRequestCapacity(table, table.Capacity() * 2);
    }
    uint64_t request_id = (uint64_t)(uintptr_t)request;
    assert(table.Get(request_id) == 0);
    table.Put(request_id, request);
    return request_id;
}

static ViewModelPropertyRequest* TakeViewModelPropertyRequest(dmHashTable<uint64_t, ViewModelPropertyRequest*>& table, uint64_t request_id)
{
    EnsureViewModelPropertyRequestCapacity(table, 1);
    DM_MUTEX_SCOPED_LOCK(g_ViewModelPropertyRequestMutex);
    ViewModelPropertyRequest** entry = table.Get(request_id);
    if (!entry)
    {
        return 0;
    }
    ViewModelPropertyRequest* request = *entry;
    table.Erase(request_id);
    return request;
}

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

void RequestViewModelInstanceProperties(rive::FileHandle file, rive::ViewModelInstanceHandle instance, const char* viewmodel_name)
{
    if (instance == RIVE_NULL_HANDLE || viewmodel_name == 0 || viewmodel_name[0] == '\0')
    {
        return;
    }

    ViewModelPropertyRequest* request = new ViewModelPropertyRequest();
    request->m_Instance = instance;
    request->m_File = file;
    uint64_t request_id = RegisterViewModelPropertyRequest(g_PendingViewModelPropertyRequests, request);
    dmRiveCommands::GetCommandQueue()->requestViewModelPropertyDefinitions(file, viewmodel_name, request_id);
}

void RequestDefaultViewModelInstanceProperties(rive::FileHandle file, rive::ArtboardHandle artboard, rive::ViewModelInstanceHandle instance)
{
    if (instance == RIVE_NULL_HANDLE || artboard == RIVE_NULL_HANDLE)
    {
        return;
    }

    ViewModelPropertyRequest* request = new ViewModelPropertyRequest();
    request->m_Instance = instance;
    request->m_File = file;
    uint64_t request_id = RegisterViewModelPropertyRequest(g_PendingDefaultViewModelRequests, request);
    dmRiveCommands::GetCommandQueue()->requestDefaultViewModelInfo(artboard, file, request_id);
}


struct DeleteRequestContext
{
};

static void DeleteRequest(DeleteRequestContext*, const uint64_t*, ViewModelPropertyRequest** request)
{
    if (request && *request)
    {
        delete *request;
        *request = 0;
    }
}

void ClearViewModelInstancePropertyRequests()
{
    EnsureViewModelPropertyRequestCapacity(g_PendingDefaultViewModelRequests, 1);
    EnsureViewModelPropertyRequestCapacity(g_PendingViewModelPropertyRequests, 1);
    DM_MUTEX_SCOPED_LOCK(g_ViewModelPropertyRequestMutex);

    DeleteRequestContext context;
    g_PendingDefaultViewModelRequests.Iterate(DeleteRequest, &context);
    g_PendingDefaultViewModelRequests.Clear();
    g_PendingViewModelPropertyRequests.Iterate(DeleteRequest, &context);
    g_PendingViewModelPropertyRequests.Clear();
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
    ViewModelPropertyRequest* request = TakeViewModelPropertyRequest(g_PendingViewModelPropertyRequests, requestId);
    if (request)
    {
        ViewModelInstanceListener* listener = GetViewModelInstanceListener(request->m_Instance);
        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
        for (uint32_t i = 0; i < properties.size(); ++i)
        {
            const ViewModelPropertyData& property = properties[i];
            queue->subscribeToViewModelProperty(request->m_Instance, property.name, property.type);
            switch (property.type)
            {
                case rive::DataType::boolean:
                    queue->requestViewModelInstanceBool(request->m_Instance, property.name);
                    break;
                case rive::DataType::number:
                case rive::DataType::integer:
                    queue->requestViewModelInstanceNumber(request->m_Instance, property.name);
                    break;
                case rive::DataType::color:
                    queue->requestViewModelInstanceColor(request->m_Instance, property.name);
                    break;
                case rive::DataType::enumType:
                    queue->requestViewModelInstanceEnum(request->m_Instance, property.name);
                    break;
                case rive::DataType::string:
                    queue->requestViewModelInstanceString(request->m_Instance, property.name);
                    break;
                case rive::DataType::list:
                    if (listener)
                        listener->EnsureListSize(dmHashString64(property.name.c_str()), 0);
                    queue->requestViewModelInstanceListSize(request->m_Instance, property.name);
                    break;
                default:
                    break;
            }
        }
        delete request;
    }

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
    ViewModelPropertyRequest* request = TakeViewModelPropertyRequest(g_PendingDefaultViewModelRequests, requestId);
    if (request)
    {
        RequestViewModelInstanceProperties(request->m_File, request->m_Instance, viewModelName.c_str());
        delete request;
    }
}
void ArtboardListener::onArtboardDeleted(const rive::ArtboardHandle, uint64_t requestId)
{

}
void ArtboardListener::onStateMachinesListed(const rive::ArtboardHandle, uint64_t requestId, std::vector<std::string> stateMachineNames)
{

}

// ******************************************************************************************************************************

ViewModelInstanceListener::ViewModelInstanceListener()
    : m_Callback(0)
    , m_Mutex(dmMutex::New())
    , m_DeleteOnViewModelDeleted(false)
{
}

ViewModelInstanceListener::~ViewModelInstanceListener()
{
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(m_Mutex);
        DeleteContext context;
        m_PropertyValues.Iterate(DeletePropertyValue, &context);
        m_PropertyValues.Clear();
        m_ListSizes.Iterate(DeleteListSize, &context);
        m_ListSizes.Clear();
    }
    if (m_Mutex)
    {
        dmMutex::Delete(m_Mutex);
        m_Mutex = 0;
    }
}

void ViewModelInstanceListener::SetAutoDeleteOnViewModelDeleted(bool value)
{
    m_DeleteOnViewModelDeleted = value;
}

bool ViewModelInstanceListener::SetPropertyValue(dmhash_t path_hash, const rive::CommandQueue::ViewModelInstanceData& data)
{
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(m_Mutex);
    if (m_PropertyValues.Capacity() == 0)
    {
        EnsureTableCapacity(m_PropertyValues, 32);
    }
    else if (m_PropertyValues.Full())
    {
        EnsureTableCapacity(m_PropertyValues, m_PropertyValues.Capacity() * 2);
    }

    rive::CommandQueue::ViewModelInstanceData** entry = m_PropertyValues.Get(path_hash);
    if (entry && *entry)
    {
        **entry = data;
        return true;
    }

    rive::CommandQueue::ViewModelInstanceData* copy = new rive::CommandQueue::ViewModelInstanceData(data);
    m_PropertyValues.Put(path_hash, copy);
    return true;
}

bool ViewModelInstanceListener::GetPropertyValue(dmhash_t path_hash, rive::CommandQueue::ViewModelInstanceData& out) const
{
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(m_Mutex);
    if (m_PropertyValues.Capacity() == 0)
    {
        return false;
    }

    rive::CommandQueue::ViewModelInstanceData* const* entry = m_PropertyValues.Get(path_hash);
    if (entry && *entry)
    {
        out = **entry;
        return true;
    }
    return false;
}

bool ViewModelInstanceListener::GetListSize(dmhash_t path_hash, size_t& out) const
{
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(m_Mutex);
    if (m_ListSizes.Capacity() == 0)
    {
        return false;
    }

    size_t* const* entry = m_ListSizes.Get(path_hash);
    if (entry && *entry)
    {
        out = **entry;
        return true;
    }
    return false;
}

bool ViewModelInstanceListener::AdjustListSize(dmhash_t path_hash, int32_t delta)
{
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(m_Mutex);
    if (m_ListSizes.Capacity() == 0)
    {
        EnsureTableCapacity(m_ListSizes, 16);
    }
    else if (m_ListSizes.Full())
    {
        EnsureTableCapacity(m_ListSizes, m_ListSizes.Capacity() * 2);
    }

    size_t* const* entry = m_ListSizes.Get(path_hash);
    if (!entry || !*entry)
    {
        size_t* copy = new size_t(0);
        m_ListSizes.Put(path_hash, copy);
        entry = m_ListSizes.Get(path_hash);
        if (!entry || !*entry)
        {
            return false;
        }
    }

    int64_t new_size = (int64_t)**entry + (int64_t)delta;
    if (new_size < 0)
    {
        new_size = 0;
    }
    **entry = (size_t)new_size;
    return true;
}

bool ViewModelInstanceListener::EnsureListSize(dmhash_t path_hash, size_t value)
{
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(m_Mutex);
    if (m_ListSizes.Capacity() == 0)
    {
        EnsureTableCapacity(m_ListSizes, 16);
    }
    else if (m_ListSizes.Full())
    {
        EnsureTableCapacity(m_ListSizes, m_ListSizes.Capacity() * 2);
    }

    size_t** entry = m_ListSizes.Get(path_hash);
    if (entry && *entry)
    {
        return true;
    }

    size_t* copy = new size_t(value);
    m_ListSizes.Put(path_hash, copy);
    return true;
}

void ViewModelInstanceListener::onViewModelInstanceError(const rive::ViewModelInstanceHandle, uint64_t requestId, std::string error)
{

}

void ViewModelInstanceListener::onViewModelDeleted(const rive::ViewModelInstanceHandle handle, uint64_t requestId)
{
    UnregisterViewModelInstanceListener(handle);
    if (m_DeleteOnViewModelDeleted)
    {
        delete this;
    }
}

void ViewModelInstanceListener::onViewModelDataReceived(const rive::ViewModelInstanceHandle, uint64_t requestId, rive::CommandQueue::ViewModelInstanceData data)
{
    dmhash_t path_hash = dmHashString64(data.metaData.name.c_str());
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(m_Mutex);
    if (m_PropertyValues.Capacity() == 0)
    {
        EnsureTableCapacity(m_PropertyValues, 32);
    }
    else if (m_PropertyValues.Full())
    {
        EnsureTableCapacity(m_PropertyValues, m_PropertyValues.Capacity() * 2);
    }

    rive::CommandQueue::ViewModelInstanceData** entry = m_PropertyValues.Get(path_hash);
    if (entry && *entry)
    {
        **entry = data;
    }
    else
    {
        rive::CommandQueue::ViewModelInstanceData* copy = new rive::CommandQueue::ViewModelInstanceData(data);
        m_PropertyValues.Put(path_hash, copy);
    }
}

void ViewModelInstanceListener::onViewModelListSizeReceived(const rive::ViewModelInstanceHandle, uint64_t requestId, std::string path, size_t size)
{
    dmhash_t path_hash = dmHashString64(path.c_str());
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(m_Mutex);
    if (m_ListSizes.Capacity() == 0)
    {
        EnsureTableCapacity(m_ListSizes, 16);
    }
    else if (m_ListSizes.Full())
    {
        EnsureTableCapacity(m_ListSizes, m_ListSizes.Capacity() * 2);
    }

    size_t** entry = m_ListSizes.Get(path_hash);
    if (entry && *entry)
    {
        **entry = size;
    }
    else
    {
        size_t* copy = new size_t(size);
        m_ListSizes.Put(path_hash, copy);
    }
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
