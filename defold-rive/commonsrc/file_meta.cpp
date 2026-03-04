// Copyright 2025 The Defold Foundation
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

#include "file_meta.h"

#include <stdlib.h>
#include <string.h>

#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/hashtable.h>
#include <dmsdk/dlib/log.h>

#include <common/commands.h>

namespace dmRive
{

#if defined(DM_RIVE_FILE_META_DATA)
static const int DM_MAX_MESSAGE_LOOPS = 256;

static void ClearStringArray(dmArray<const char*>& array)
{
    for (uint32_t i = 0; i < array.Size(); ++i)
    {
        free((void*)array[i]);
    }
    array.SetSize(0);
}

static void ClearDefaultViewModelInfo(DefaultViewModelInfo& info)
{
    free((void*)info.m_ViewModel);
    free((void*)info.m_Instance);
    info.m_ViewModel = 0;
    info.m_Instance = 0;
}

static void ClearViewModelProperties(dmArray<ViewModelProperty>& array)
{
    for (uint32_t i = 0; i < array.Size(); ++i)
    {
        free((void*)array[i].m_ViewModel);
        free((void*)array[i].m_Name);
        free((void*)array[i].m_MetaData);
        free((void*)array[i].m_Value);
        array[i].m_ViewModel = 0;
        array[i].m_Name = 0;
        array[i].m_MetaData = 0;
        array[i].m_Value = 0;
    }
    array.SetSize(0);
}

static void ClearViewModelEnums(dmArray<ViewModelEnum>& array)
{
    for (uint32_t i = 0; i < array.Size(); ++i)
    {
        free((void*)array[i].m_Name);
        ClearStringArray(array[i].m_Enumerants);
        array[i].m_Name = 0;
    }
    array.SetSize(0);
}

static void ClearViewModelInstanceNames(dmArray<ViewModelInstanceNames>& array)
{
    for (uint32_t i = 0; i < array.Size(); ++i)
    {
        free((void*)array[i].m_ViewModel);
        ClearStringArray(array[i].m_Instances);
        array[i].m_ViewModel = 0;
    }
    array.SetSize(0);
}

static void ClearArtboardStateMachines(dmArray<ArtboardStateMachines>& array)
{
    for (uint32_t i = 0; i < array.Size(); ++i)
    {
        free((void*)array[i].m_Artboard);
        ClearStringArray(array[i].m_StateMachines);
        array[i].m_Artboard = 0;
    }
    array.SetSize(0);
}

static void FillStringArray(dmArray<const char*>& array, const std::vector<std::string>& values)
{
    ClearStringArray(array);
    if (!values.empty())
    {
        array.SetCapacity((uint32_t)values.size());
        array.SetSize((uint32_t)values.size());
        for (uint32_t i = 0; i < values.size(); ++i)
        {
            array[i] = strdup(values[i].c_str());
        }
    }
}

static void LogNameList(const char* label, const dmArray<const char*>& names)
{
    dmLogInfo("%s (%u)", label, names.Size());
    for (uint32_t i = 0; i < names.Size(); ++i)
    {
        const char* name = names[i] ? names[i] : "<null>";
        dmLogInfo("  - '%s'", name);
    }
}
#endif // DM_RIVE_FILE_META_DATA

#if defined(DM_RIVE_FILE_META_DATA)
class ArtboardMetadataListener : public rive::CommandQueue::ArtboardListener
{
public:
    ArtboardMetadataListener();
    explicit ArtboardMetadataListener(RiveFile* file);

    void onArtboardError(const rive::ArtboardHandle artboard,
                         uint64_t request_id,
                         std::string error) override;
    void onStateMachinesListed(const rive::ArtboardHandle artboard,
                               uint64_t request_id,
                               std::vector<std::string> stateMachineNames) override;
    void onDefaultViewModelInfoReceived(const rive::ArtboardHandle artboard,
                                        uint64_t request_id,
                                        std::string viewModelName,
                                        std::string instanceName) override;

    uint32_t m_StateMachinesListedCount;
    uint32_t m_StateMachinesExpectedCount;
    bool m_ArtboardError;
    bool m_DefaultViewModelInfoReceived;
    RiveFile* m_File;
};

class ViewModelInstanceMetadataListener : public rive::CommandQueue::ViewModelInstanceListener
{
public:
    explicit ViewModelInstanceMetadataListener(RiveFile* file);

    uint64_t RegisterRequest(uint32_t property_index);
    uint32_t GetPendingCount() const;

    void onViewModelInstanceError(const rive::ViewModelInstanceHandle handle,
                                  uint64_t requestId,
                                  std::string error) override;
    void onViewModelDeleted(const rive::ViewModelInstanceHandle handle,
                            uint64_t requestId) override;
    void onViewModelDataReceived(const rive::ViewModelInstanceHandle handle,
                                 uint64_t requestId,
                                 rive::CommandQueue::ViewModelInstanceData data) override;
    void onViewModelListSizeReceived(const rive::ViewModelInstanceHandle handle,
                                     uint64_t requestId,
                                     std::string path,
                                     size_t size) override;

private:
    void ResolvePendingRequest(uint64_t requestId,
                               const rive::CommandQueue::ViewModelInstanceData* data,
                               const size_t* list_size,
                               const char* fallback_value);

    RiveFile* m_File;
    dmHashTable64<uint32_t> m_PendingRequests;
    uint64_t m_NextRequestId;
};
#endif // DM_RIVE_FILE_META_DATA

#if defined(DM_RIVE_FILE_META_DATA)
MetadataListener::MetadataListener()
: m_FileLoaded(false)
, m_FileError(false)
, m_ArtboardsListed(false)
, m_ViewModelsListed(false)
, m_ViewModelEnumsListed(false)
, m_ViewModelPropertiesListedCount(0)
, m_ViewModelPropertiesExpectedCount(0)
, m_ViewModelInstanceNamesListedCount(0)
, m_ViewModelInstanceNamesExpectedCount(0)
, m_File(0)
{
}

MetadataListener::MetadataListener(RiveFile* file)
: m_FileLoaded(false)
, m_FileError(false)
, m_ArtboardsListed(false)
, m_ViewModelsListed(false)
, m_ViewModelEnumsListed(false)
, m_ViewModelPropertiesListedCount(0)
, m_ViewModelPropertiesExpectedCount(0)
, m_ViewModelInstanceNamesListedCount(0)
, m_ViewModelInstanceNamesExpectedCount(0)
, m_File(file)
{
}
#else
MetadataListener::MetadataListener()
: m_FileLoaded(false)
, m_FileError(false)
, m_File(0)
{
}

MetadataListener::MetadataListener(RiveFile* file)
: m_FileLoaded(false)
, m_FileError(false)
, m_File(file)
{
}
#endif

void MetadataListener::onFileLoaded(const rive::FileHandle, uint64_t)
{
    m_FileLoaded = true;
}

void MetadataListener::onFileError(const rive::FileHandle, uint64_t, std::string error)
{
    dmLogError("Rive file load error: %s", error.c_str());
    m_FileError = true;
}

#if defined(DM_RIVE_FILE_META_DATA)
void MetadataListener::onArtboardsListed(const rive::FileHandle,
                                         uint64_t,
                                         std::vector<std::string> artboardNames)
{
    FillStringArray(m_File->m_Artboards, artboardNames);
    m_ArtboardsListed = true;
}

void MetadataListener::onViewModelsListed(const rive::FileHandle,
                                          uint64_t,
                                          std::vector<std::string> viewModelNames)
{
    FillStringArray(m_File->m_ViewModels, viewModelNames);
    m_ViewModelsListed = true;
}

void MetadataListener::onViewModelInstanceNamesListed(const rive::FileHandle,
                                                      uint64_t,
                                                      std::string viewModelName,
                                                      std::vector<std::string> instanceNames)
{
    uint32_t index = m_File->m_ViewModelInstanceNames.Size();
    uint32_t new_size = index + 1;
    if (m_File->m_ViewModelInstanceNames.Capacity() < new_size)
    {
        m_File->m_ViewModelInstanceNames.SetCapacity(new_size);
    }
    m_File->m_ViewModelInstanceNames.SetSize(new_size);

    ViewModelInstanceNames* entry = &m_File->m_ViewModelInstanceNames[index];
    memset(entry, 0, sizeof(*entry));
    entry->m_ViewModel = strdup(viewModelName.c_str());
    FillStringArray(entry->m_Instances, instanceNames);

    ++m_ViewModelInstanceNamesListedCount;
}

void MetadataListener::onViewModelPropertiesListed(
    const rive::FileHandle,
    uint64_t,
    std::string viewModelName,
    std::vector<ViewModelPropertyData> properties)
{
    uint32_t count = properties.size();
    if (count > 0)
    {
        uint32_t index = m_File->m_ViewModelProperties.Size();
        uint32_t new_size = index + count;
        if (m_File->m_ViewModelProperties.Capacity() < new_size)
        {
            m_File->m_ViewModelProperties.SetCapacity(new_size);
        }
        m_File->m_ViewModelProperties.SetSize(new_size);
        for (uint32_t i = 0; i < count; ++i)
        {
            ViewModelProperty* entry = &m_File->m_ViewModelProperties[index + i];
            memset(entry, 0, sizeof(*entry));
            entry->m_ViewModel = strdup(viewModelName.c_str());
            entry->m_Name = strdup(properties[i].name.c_str());
            entry->m_MetaData = strdup(properties[i].metaData.c_str());
            entry->m_Type = properties[i].type;
            entry->m_Value = 0;
        }
    }
    ++m_ViewModelPropertiesListedCount;
}

void MetadataListener::onViewModelEnumsListed(const rive::FileHandle,
                                              uint64_t,
                                              std::vector<rive::ViewModelEnum> enums)
{
    ClearViewModelEnums(m_File->m_ViewModelEnums);
    uint32_t count = enums.size();
    if (count > 0)
    {
        m_File->m_ViewModelEnums.SetCapacity((uint32_t)enums.size());
        m_File->m_ViewModelEnums.SetSize((uint32_t)enums.size());
        for (uint32_t i = 0; i < enums.size(); ++i)
        {
            ViewModelEnum* entry = &m_File->m_ViewModelEnums[i];
            memset(entry, 0, sizeof(*entry));
            entry->m_Name = strdup(enums[i].name.c_str());
            FillStringArray(entry->m_Enumerants, enums[i].enumerants);
        }
    }
    m_ViewModelEnumsListed = true;
}
#endif // DM_RIVE_FILE_META_DATA

#if defined(DM_RIVE_FILE_META_DATA)
ArtboardMetadataListener::ArtboardMetadataListener()
: m_StateMachinesListedCount(0)
, m_StateMachinesExpectedCount(0)
, m_ArtboardError(false)
, m_DefaultViewModelInfoReceived(false)
, m_File(0)
{
}

ArtboardMetadataListener::ArtboardMetadataListener(RiveFile* file)
: m_StateMachinesListedCount(0)
, m_StateMachinesExpectedCount(0)
, m_ArtboardError(false)
, m_DefaultViewModelInfoReceived(false)
, m_File(file)
{
}

void ArtboardMetadataListener::onArtboardError(const rive::ArtboardHandle,
                                               uint64_t,
                                               std::string error)
{
    dmLogError("Rive artboard error: %s", error.c_str());
    m_ArtboardError = true;
}

void ArtboardMetadataListener::onStateMachinesListed(const rive::ArtboardHandle,
                                                     uint64_t request_id,
                                                     std::vector<std::string> stateMachineNames)
{
    ++m_StateMachinesListedCount;
    if (!m_File)
    {
        return;
    }

    const char* artboard_name = 0;
    uint32_t index = (uint32_t)request_id;
    if (index < m_File->m_Artboards.Size())
    {
        artboard_name = m_File->m_Artboards[index];
    }

    if (!artboard_name)
    {
        artboard_name = "";
    }

    uint32_t entry_index = m_File->m_StateMachinesByArtboard.Size();
    uint32_t new_size = entry_index + 1;
    if (m_File->m_StateMachinesByArtboard.Capacity() < new_size)
    {
        m_File->m_StateMachinesByArtboard.SetCapacity(new_size);
    }
    m_File->m_StateMachinesByArtboard.SetSize(new_size);

    ArtboardStateMachines* entry = &m_File->m_StateMachinesByArtboard[entry_index];
    memset(entry, 0, sizeof(*entry));
    entry->m_Artboard = strdup(artboard_name);
    FillStringArray(entry->m_StateMachines, stateMachineNames);
}

void ArtboardMetadataListener::onDefaultViewModelInfoReceived(const rive::ArtboardHandle,
                                                              uint64_t,
                                                              std::string viewModelName,
                                                              std::string instanceName)
{
    ClearDefaultViewModelInfo(m_File->m_DefaultViewModelInfo);
    m_File->m_DefaultViewModelInfo.m_ViewModel = strdup(viewModelName.c_str());
    m_File->m_DefaultViewModelInfo.m_Instance = strdup(instanceName.c_str());
    m_File->m_HasDefaultViewModelInfo = true;
    m_DefaultViewModelInfoReceived = true;
}

ViewModelInstanceMetadataListener::ViewModelInstanceMetadataListener(RiveFile* file)
: m_File(file)
, m_NextRequestId(1)
{
    m_PendingRequests.OffsetCapacity(32);
}

uint64_t ViewModelInstanceMetadataListener::RegisterRequest(uint32_t property_index)
{
    uint64_t request_id = m_NextRequestId++;

    if (m_PendingRequests.Full())
    {
        m_PendingRequests.OffsetCapacity(32);
    }
    m_PendingRequests.Put(request_id, property_index);
    return request_id;
}

uint32_t ViewModelInstanceMetadataListener::GetPendingCount() const
{
    return m_PendingRequests.Size();
}

void ViewModelInstanceMetadataListener::onViewModelInstanceError(const rive::ViewModelInstanceHandle,
                                                                 uint64_t requestId,
                                                                 std::string error)
{
    dmLogError("Rive view model instance error: %s", error.c_str());
    ResolvePendingRequest(requestId, 0, 0, "<error>");
}

void ViewModelInstanceMetadataListener::onViewModelDeleted(const rive::ViewModelInstanceHandle,
                                                           uint64_t requestId)
{
    ResolvePendingRequest(requestId, 0, 0, "<deleted>");
}

void ViewModelInstanceMetadataListener::onViewModelDataReceived(const rive::ViewModelInstanceHandle,
                                                                uint64_t requestId,
                                                                rive::CommandQueue::ViewModelInstanceData data)
{
    ResolvePendingRequest(requestId, &data, 0, 0);
}

void ViewModelInstanceMetadataListener::onViewModelListSizeReceived(const rive::ViewModelInstanceHandle,
                                                                    uint64_t requestId,
                                                                    std::string,
                                                                    size_t size)
{
    ResolvePendingRequest(requestId, 0, &size, 0);
}

void ViewModelInstanceMetadataListener::ResolvePendingRequest(uint64_t requestId,
                                                              const rive::CommandQueue::ViewModelInstanceData* data,
                                                              const size_t* list_size,
                                                              const char* fallback_value)
{
    uint32_t* property_index = m_PendingRequests.Get(requestId);
    if (!property_index)
    {
        return;
    }

    if (!m_File || *property_index >= m_File->m_ViewModelProperties.Size())
    {
        m_PendingRequests.Erase(requestId);
        return;
    }

    ViewModelProperty* property = &m_File->m_ViewModelProperties[*property_index];
    const char* value_text = fallback_value;
    char buffer[64];

    if (data)
    {
        switch (data->metaData.type)
        {
            case rive::DataType::boolean:
                value_text = data->boolValue ? "true" : "false";
                break;
            case rive::DataType::number:
            case rive::DataType::integer:
                dmSnPrintf(buffer, sizeof(buffer), "%g", data->numberValue);
                value_text = buffer;
                break;
            case rive::DataType::color:
                dmSnPrintf(buffer, sizeof(buffer), "#%08X", (uint32_t)data->colorValue);
                value_text = buffer;
                break;
            case rive::DataType::enumType:
            case rive::DataType::string:
                value_text = data->stringValue.c_str();
                break;
            case rive::DataType::list:
                if (list_size)
                {
                    dmSnPrintf(buffer, sizeof(buffer), "%zu", *list_size);
                    value_text = buffer;
                }
                break;
            default:
                break;
        }
    }
    else if (list_size)
    {
        dmSnPrintf(buffer, sizeof(buffer), "%zu", *list_size);
        value_text = buffer;
    }

    if (value_text)
    {
        free((void*)property->m_Value);
        property->m_Value = strdup(value_text);
    }

    m_PendingRequests.Erase(requestId);
}
#endif // DM_RIVE_FILE_META_DATA

#if defined(DM_RIVE_FILE_META_DATA)
void InitFileMetaData(RiveFile* file)
{
    file->m_Artboards.SetSize(0);
    file->m_StateMachinesByArtboard.SetSize(0);
    file->m_ViewModels.SetSize(0);
    file->m_ViewModelProperties.SetSize(0);
    file->m_ViewModelEnums.SetSize(0);
    file->m_ViewModelInstanceNames.SetSize(0);
    file->m_DefaultViewModelInfo.m_ViewModel = 0;
    file->m_DefaultViewModelInfo.m_Instance = 0;
    file->m_HasDefaultViewModelInfo = false;
    file->m_SelectedArtboardName = 0;
    file->m_ArtboardListener = new ArtboardMetadataListener(file);
}

void DestroyFileMetaData(RiveFile* file)
{
    ClearStringArray(file->m_Artboards);
    ClearStringArray(file->m_ViewModels);
    ClearArtboardStateMachines(file->m_StateMachinesByArtboard);
    ClearViewModelProperties(file->m_ViewModelProperties);
    ClearViewModelEnums(file->m_ViewModelEnums);
    ClearViewModelInstanceNames(file->m_ViewModelInstanceNames);
    ClearDefaultViewModelInfo(file->m_DefaultViewModelInfo);
    free((void*)file->m_SelectedArtboardName);
    file->m_SelectedArtboardName = 0;
    delete file->m_ArtboardListener;
}

void RequestMetaData(RiveFile* file, rive::rcp<rive::CommandQueue> queue)
{
    MetadataListener* listener = file->m_FileListener;

    queue->requestArtboardNames(file->m_File);
    queue->requestViewModelNames(file->m_File);
    for (int i = 0; i < DM_MAX_MESSAGE_LOOPS &&
            (!listener->m_ArtboardsListed || !listener->m_ViewModelsListed) && !listener->m_FileError; ++i)
    {
        dmRiveCommands::ProcessMessages();
    }

    ClearViewModelProperties(file->m_ViewModelProperties);
    ClearViewModelEnums(file->m_ViewModelEnums);
    ClearViewModelInstanceNames(file->m_ViewModelInstanceNames);
    listener->m_ViewModelEnumsListed = false;
    listener->m_ViewModelPropertiesListedCount = 0;
    listener->m_ViewModelInstanceNamesListedCount = 0;
    listener->m_ViewModelPropertiesExpectedCount = file->m_ViewModels.Size();
    listener->m_ViewModelInstanceNamesExpectedCount = file->m_ViewModels.Size();

    queue->requestViewModelEnums(file->m_File);
    for (uint32_t i = 0; i < file->m_ViewModels.Size(); ++i)
    {
        queue->requestViewModelPropertyDefinitions(file->m_File, file->m_ViewModels[i]);
        queue->requestViewModelInstanceNames(file->m_File, file->m_ViewModels[i]);
    }

    for (int i = 0; i < DM_MAX_MESSAGE_LOOPS &&
            (!listener->m_ViewModelEnumsListed ||
             listener->m_ViewModelPropertiesListedCount < listener->m_ViewModelPropertiesExpectedCount ||
             listener->m_ViewModelInstanceNamesListedCount < listener->m_ViewModelInstanceNamesExpectedCount) &&
             !listener->m_FileError; ++i)
    {
        dmRiveCommands::ProcessMessages();
    }

    dmArray<ViewModelInstanceMetadataListener*> view_model_listeners;
    if (file->m_ViewModels.Size() > 0)
    {
        view_model_listeners.SetCapacity(file->m_ViewModels.Size());
    }

    dmArray<rive::ViewModelInstanceHandle> view_models_to_delete;
    if (file->m_ViewModels.Size() > 0)
    {
        view_models_to_delete.SetCapacity(file->m_ViewModels.Size());
    }

    for (uint32_t i = 0; i < file->m_ViewModels.Size(); ++i)
    {
        const char* view_model_name = file->m_ViewModels[i];
        ViewModelInstanceMetadataListener* view_model_listener = new ViewModelInstanceMetadataListener(file);
        rive::ViewModelInstanceHandle instance = queue->instantiateDefaultViewModelInstance(file->m_File,
                                                                                            view_model_name,
                                                                                            view_model_listener);
        if (instance == RIVE_NULL_HANDLE)
        {
            delete view_model_listener;
            continue;
        }

        uint32_t listener_index = view_model_listeners.Size();
        view_model_listeners.SetSize(listener_index + 1);
        view_model_listeners[listener_index] = view_model_listener;

        uint32_t handle_index = view_models_to_delete.Size();
        view_models_to_delete.SetSize(handle_index + 1);
        view_models_to_delete[handle_index] = instance;

        for (uint32_t p = 0; p < file->m_ViewModelProperties.Size(); ++p)
        {
            ViewModelProperty* property = &file->m_ViewModelProperties[p];
            if (!property->m_ViewModel || strcmp(property->m_ViewModel, view_model_name) != 0)
            {
                continue;
            }

            const char* property_name = property->m_Name ? property->m_Name : "";
            switch (property->m_Type)
            {
                case rive::DataType::boolean:
                {
                    uint64_t request_id = view_model_listener->RegisterRequest(p);
                    queue->requestViewModelInstanceBool(instance, property_name, request_id);
                    break;
                }
                case rive::DataType::number:
                case rive::DataType::integer:
                {
                    uint64_t request_id = view_model_listener->RegisterRequest(p);
                    queue->requestViewModelInstanceNumber(instance, property_name, request_id);
                    break;
                }
                case rive::DataType::color:
                {
                    uint64_t request_id = view_model_listener->RegisterRequest(p);
                    queue->requestViewModelInstanceColor(instance, property_name, request_id);
                    break;
                }
                case rive::DataType::enumType:
                {
                    uint64_t request_id = view_model_listener->RegisterRequest(p);
                    queue->requestViewModelInstanceEnum(instance, property_name, request_id);
                    break;
                }
                case rive::DataType::string:
                {
                    uint64_t request_id = view_model_listener->RegisterRequest(p);
                    queue->requestViewModelInstanceString(instance, property_name, request_id);
                    break;
                }
                case rive::DataType::list:
                {
                    uint64_t request_id = view_model_listener->RegisterRequest(p);
                    queue->requestViewModelInstanceListSize(instance, property_name, request_id);
                    break;
                }
                default:
                    free((void*)property->m_Value);
                    property->m_Value = strdup("<n/a>");
                    break;
            }
        }
    }

    uint32_t view_model_pending_count = 0;
    for (uint32_t i = 0; i < view_model_listeners.Size(); ++i)
    {
        view_model_pending_count += view_model_listeners[i]->GetPendingCount();
    }

    for (int i = 0; i < DM_MAX_MESSAGE_LOOPS && view_model_pending_count > 0; ++i)
    {
        dmRiveCommands::ProcessMessages();

        view_model_pending_count = 0;
        for (uint32_t j = 0; j < view_model_listeners.Size(); ++j)
        {
            view_model_pending_count += view_model_listeners[j]->GetPendingCount();
        }
    }

    for (uint32_t i = 0; i < view_models_to_delete.Size(); ++i)
    {
        queue->deleteViewModelInstance(view_models_to_delete[i]);
    }
    if (view_models_to_delete.Size() > 0)
    {
        dmRiveCommands::ProcessMessages();
    }

    for (uint32_t i = 0; i < view_model_listeners.Size(); ++i)
    {
        delete view_model_listeners[i];
    }

    ClearArtboardStateMachines(file->m_StateMachinesByArtboard);
    uint32_t state_machines_completed_count = 0;
    bool artboard_error = false;

    dmArray<ArtboardMetadataListener*> artboard_listeners;
    if (file->m_Artboards.Size() > 0)
    {
        artboard_listeners.SetCapacity(file->m_Artboards.Size());
    }

    dmArray<rive::ArtboardHandle> artboards_to_delete;
    if (file->m_Artboards.Size() > 0)
    {
        artboards_to_delete.SetCapacity(file->m_Artboards.Size());
    }

    for (uint32_t i = 0; i < file->m_Artboards.Size(); ++i)
    {
        const char* artboard_name = file->m_Artboards[i];
        ArtboardMetadataListener* artboard_listener = new ArtboardMetadataListener(file);
        rive::ArtboardHandle artboard = queue->instantiateArtboardNamed(file->m_File, artboard_name, artboard_listener);
        if (artboard != RIVE_NULL_HANDLE)
        {
            uint32_t listener_index = artboard_listeners.Size();
            artboard_listeners.SetSize(listener_index + 1);
            artboard_listeners[listener_index] = artboard_listener;

            uint32_t handle_index = artboards_to_delete.Size();
            artboards_to_delete.SetSize(handle_index + 1);
            artboards_to_delete[handle_index] = artboard;
            queue->requestStateMachineNames(artboard, i);
        }
        else
        {
            delete artboard_listener;

            uint32_t entry_index = file->m_StateMachinesByArtboard.Size();
            uint32_t new_size = entry_index + 1;
            if (file->m_StateMachinesByArtboard.Capacity() < new_size)
            {
                file->m_StateMachinesByArtboard.SetCapacity(new_size);
            }
            file->m_StateMachinesByArtboard.SetSize(new_size);
            ArtboardStateMachines* entry = &file->m_StateMachinesByArtboard[entry_index];
            memset(entry, 0, sizeof(*entry));
            entry->m_Artboard = artboard_name ? strdup(artboard_name) : strdup("");
            entry->m_StateMachines.SetSize(0);
            ++state_machines_completed_count;
        }
    }

    for (int i = 0; i < DM_MAX_MESSAGE_LOOPS &&
            state_machines_completed_count < file->m_Artboards.Size() &&
            !artboard_error; ++i)
    {
        dmRiveCommands::ProcessMessages();

        state_machines_completed_count = 0;
        artboard_error = false;
        for (uint32_t j = 0; j < artboard_listeners.Size(); ++j)
        {
            ArtboardMetadataListener* artboard_listener = artboard_listeners[j];
            if (artboard_listener->m_StateMachinesListedCount > 0)
            {
                ++state_machines_completed_count;
            }
            artboard_error = artboard_error || artboard_listener->m_ArtboardError;
        }

        // Include artboards that failed to instantiate, which were already
        // counted and inserted as empty entries.
        state_machines_completed_count += file->m_Artboards.Size() - artboard_listeners.Size();
    }

    for (uint32_t i = 0; i < artboards_to_delete.Size(); ++i)
    {
        queue->deleteArtboard(artboards_to_delete[i]);
    }
    if (artboards_to_delete.Size() > 0)
    {
        dmRiveCommands::ProcessMessages();
    }

    for (uint32_t i = 0; i < artboard_listeners.Size(); ++i)
    {
        delete artboard_listeners[i];
    }
}

rive::ArtboardHandle InstantiateArtboardNamedWithMeta(RiveFile* file, const char* artboard, rive::rcp<rive::CommandQueue> queue)
{
    ArtboardMetadataListener* artboard_listener = file->m_ArtboardListener;
    const char* artboard_name = artboard ? artboard : "";
    rive::ArtboardHandle artboard_handle = queue->instantiateArtboardNamed(file->m_File, artboard_name, artboard_listener);
    if (artboard_handle == RIVE_NULL_HANDLE)
    {
        return artboard_handle;
    }

    ClearDefaultViewModelInfo(file->m_DefaultViewModelInfo);
    file->m_HasDefaultViewModelInfo = false;
    if (file->m_ViewModels.Size() == 0)
    {
        return artboard_handle;
    }
    artboard_listener->m_DefaultViewModelInfoReceived = false;
    artboard_listener->m_ArtboardError = false;
    queue->requestDefaultViewModelInfo(artboard_handle, file->m_File);
    for (int i = 0; i < DM_MAX_MESSAGE_LOOPS &&
            !artboard_listener->m_DefaultViewModelInfoReceived &&
            !artboard_listener->m_ArtboardError; ++i)
    {
        dmRiveCommands::ProcessMessages();
    }

    return artboard_handle;
}

rive::ArtboardHandle InstantiateDefaultArtboardWithMeta(RiveFile* file, rive::rcp<rive::CommandQueue> queue)
{
    ArtboardMetadataListener* artboard_listener = file->m_ArtboardListener;
    rive::ArtboardHandle artboard = queue->instantiateDefaultArtboard(file->m_File, artboard_listener);
    if (artboard == RIVE_NULL_HANDLE)
    {
        return artboard;
    }

    ClearDefaultViewModelInfo(file->m_DefaultViewModelInfo);
    file->m_HasDefaultViewModelInfo = false;
    if (file->m_ViewModels.Size() == 0)
    {
        return artboard;
    }
    artboard_listener->m_DefaultViewModelInfoReceived = false;
    artboard_listener->m_ArtboardError = false;
    queue->requestDefaultViewModelInfo(artboard, file->m_File);
    for (int i = 0; i < DM_MAX_MESSAGE_LOOPS &&
            !artboard_listener->m_DefaultViewModelInfoReceived &&
            !artboard_listener->m_ArtboardError; ++i)
    {
        dmRiveCommands::ProcessMessages();
    }

    return artboard;
}

void DebugPrintFileMetaData(const RiveFile* file)
{
    LogNameList("RiveFile: artboards", file->m_Artboards);
    for (uint32_t i = 0; i < file->m_StateMachinesByArtboard.Size(); ++i)
    {
        const ArtboardStateMachines& entry = file->m_StateMachinesByArtboard[i];
        const char* artboard_name = entry.m_Artboard ? entry.m_Artboard : "<null>";
        dmLogInfo("RiveFile: state machines for artboard '%s' (%u)", artboard_name, entry.m_StateMachines.Size());
        for (uint32_t j = 0; j < entry.m_StateMachines.Size(); ++j)
        {
            const char* name = entry.m_StateMachines[j] ? entry.m_StateMachines[j] : "<null>";
            dmLogInfo("  - '%s'", name);
        }
    }
    LogNameList("RiveFile: view models", file->m_ViewModels);
    if (file->m_HasDefaultViewModelInfo)
    {
        dmLogInfo("RiveFile: default view model = '%s' instance = '%s'",
                  file->m_DefaultViewModelInfo.m_ViewModel ? file->m_DefaultViewModelInfo.m_ViewModel : "<null>",
                  file->m_DefaultViewModelInfo.m_Instance ? file->m_DefaultViewModelInfo.m_Instance : "<null>");
    }
}
#endif // DM_RIVE_FILE_META_DATA

} // namespace dmRive
