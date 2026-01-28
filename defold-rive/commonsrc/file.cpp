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

#include "common/file.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dmsdk/dlib/log.h>

#include <common/commands.h>

namespace dmRive
{

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
    if (info.m_ViewModel)
    {
        free((void*)info.m_ViewModel);
    }
    if (info.m_Instance)
    {
        free((void*)info.m_Instance);
    }
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
        array[i].m_ViewModel = 0;
        array[i].m_Name = 0;
        array[i].m_MetaData = 0;
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

class MetadataListener : public rive::CommandQueue::FileListener
{
public:
    MetadataListener();
    explicit MetadataListener(RiveFile* file);

    void onFileLoaded(const rive::FileHandle file, uint64_t request_id) override;
    void onFileError(const rive::FileHandle file, uint64_t request_id, std::string error) override;
    void onArtboardsListed(const rive::FileHandle file,
                           uint64_t request_id,
                           std::vector<std::string> artboardNames) override;
    void onViewModelsListed(const rive::FileHandle file,
                            uint64_t request_id,
                            std::vector<std::string> viewModelNames) override;
    void onViewModelInstanceNamesListed(const rive::FileHandle file,
                                        uint64_t request_id,
                                        std::string viewModelName,
                                        std::vector<std::string> instanceNames) override;
    void onViewModelPropertiesListed(
        const rive::FileHandle file,
        uint64_t request_id,
        std::string viewModelName,
        std::vector<ViewModelPropertyData> properties) override;
    void onViewModelEnumsListed(const rive::FileHandle file,
                                uint64_t request_id,
                                std::vector<rive::ViewModelEnum> enums) override;

    bool m_FileLoaded;
    bool m_FileError;
    bool m_ArtboardsListed;
    bool m_ViewModelsListed;
    bool m_ViewModelEnumsListed;
    uint32_t m_ViewModelPropertiesListedCount;
    uint32_t m_ViewModelPropertiesExpectedCount;
    uint32_t m_ViewModelInstanceNamesListedCount;
    uint32_t m_ViewModelInstanceNamesExpectedCount;
    RiveFile* m_File;
};

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

void MetadataListener::onFileLoaded(const rive::FileHandle, uint64_t)
{
    m_FileLoaded = true;
}

void MetadataListener::onFileError(const rive::FileHandle, uint64_t, std::string error)
{
    dmLogError("Rive file load error: %s", error.c_str());
    m_FileError = true;
}

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
        }
    }
    ++m_ViewModelPropertiesListedCount;
}

void MetadataListener::onViewModelEnumsListed(const rive::FileHandle,
                                              uint64_t,
                                              std::vector<rive::ViewModelEnum> enums)
{
    ClearViewModelEnums(m_File->m_ViewModelEnums);
    if (!enums.empty())
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

RiveFile* LoadFileFromBuffer(const void* buffer, size_t buffer_size, const char* path)
{
    if (!buffer || buffer_size == 0)
    {
        return 0;
    }

    // rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    // if (!queue)
    // {
    //     return 0;
    // }

    std::vector<uint8_t> bytes((const uint8_t*)buffer, (const uint8_t*)buffer + buffer_size);

    RiveFile* out = new RiveFile;
    out->m_Path = strdup(path);
    out->m_Artboards.SetSize(0);
    out->m_StateMachinesByArtboard.SetSize(0);
    out->m_ViewModels.SetSize(0);
    out->m_ViewModelProperties.SetSize(0);
    out->m_ViewModelEnums.SetSize(0);
    out->m_ViewModelInstanceNames.SetSize(0);
    out->m_DefaultViewModelInfo.m_ViewModel = 0;
    out->m_DefaultViewModelInfo.m_Instance = 0;
    out->m_HasDefaultViewModelInfo = false;
    out->m_File = RIVE_NULL_HANDLE;
    out->m_Artboard = RIVE_NULL_HANDLE;
    out->m_StateMachine = RIVE_NULL_HANDLE;
    out->m_ViewModelInstance = RIVE_NULL_HANDLE;
    out->m_FileListener = new MetadataListener(out);
    out->m_ArtboardListener = new ArtboardMetadataListener(out);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();

    MetadataListener* listener = out->m_FileListener;
    out->m_File = queue->loadFile(bytes, listener);

    for (int i = 0; i < DM_MAX_MESSAGE_LOOPS && !listener->m_FileLoaded && !listener->m_FileError; ++i)
    {
        dmRiveCommands::ProcessMessages();
    }

    if (listener->m_FileError || !listener->m_FileLoaded)
    {
        if (out->m_File != RIVE_NULL_HANDLE)
        {
            queue->deleteFile(out->m_File);
            dmRiveCommands::ProcessMessages();
            out->m_File = RIVE_NULL_HANDLE;
        }
        DestroyFile(out);
        return 0;
    }

    queue->requestArtboardNames(out->m_File);
    queue->requestViewModelNames(out->m_File);
    for (int i = 0; i < DM_MAX_MESSAGE_LOOPS && (!listener->m_ArtboardsListed || !listener->m_ViewModelsListed) && !listener->m_FileError; ++i)
    {
        dmRiveCommands::ProcessMessages();
    }

#if defined(DM_RIVE_PLUGIN) || defined(DM_RIVE_VIEWER)
    ClearViewModelProperties(out->m_ViewModelProperties);
    ClearViewModelEnums(out->m_ViewModelEnums);
    ClearViewModelInstanceNames(out->m_ViewModelInstanceNames);
    listener->m_ViewModelEnumsListed = false;
    listener->m_ViewModelPropertiesListedCount = 0;
    listener->m_ViewModelInstanceNamesListedCount = 0;
    listener->m_ViewModelPropertiesExpectedCount = out->m_ViewModels.Size();
    listener->m_ViewModelInstanceNamesExpectedCount = out->m_ViewModels.Size();

    queue->requestViewModelEnums(out->m_File);
    for (uint32_t i = 0; i < out->m_ViewModels.Size(); ++i)
    {
        queue->requestViewModelPropertyDefinitions(out->m_File, out->m_ViewModels[i]);
        queue->requestViewModelInstanceNames(out->m_File, out->m_ViewModels[i]);
    }

    for (int i = 0; i < DM_MAX_MESSAGE_LOOPS &&
            (!listener->m_ViewModelEnumsListed ||
             listener->m_ViewModelPropertiesListedCount < listener->m_ViewModelPropertiesExpectedCount ||
             listener->m_ViewModelInstanceNamesListedCount < listener->m_ViewModelInstanceNamesExpectedCount) &&
             !listener->m_FileError; ++i)
    {
        dmRiveCommands::ProcessMessages();
    }
#endif

    ArtboardMetadataListener* artboard_listener = out->m_ArtboardListener;
    ClearArtboardStateMachines(out->m_StateMachinesByArtboard);
    artboard_listener->m_StateMachinesListedCount = 0;
    artboard_listener->m_StateMachinesExpectedCount = out->m_Artboards.Size();
    artboard_listener->m_ArtboardError = false;

    dmArray<rive::ArtboardHandle> artboards_to_delete;
    if (out->m_Artboards.Size() > 0)
    {
        artboards_to_delete.SetCapacity(out->m_Artboards.Size());
    }

    for (uint32_t i = 0; i < out->m_Artboards.Size(); ++i)
    {
        const char* artboard_name = out->m_Artboards[i];
        rive::ArtboardHandle artboard = queue->instantiateArtboardNamed(out->m_File, artboard_name, artboard_listener);
        if (artboard != RIVE_NULL_HANDLE)
        {
            uint32_t handle_index = artboards_to_delete.Size();
            artboards_to_delete.SetSize(handle_index + 1);
            artboards_to_delete[handle_index] = artboard;
            queue->requestStateMachineNames(artboard, i);
        }
        else
        {
            uint32_t entry_index = out->m_StateMachinesByArtboard.Size();
            uint32_t new_size = entry_index + 1;
            if (out->m_StateMachinesByArtboard.Capacity() < new_size)
            {
                out->m_StateMachinesByArtboard.SetCapacity(new_size);
            }
            out->m_StateMachinesByArtboard.SetSize(new_size);
            ArtboardStateMachines* entry = &out->m_StateMachinesByArtboard[entry_index];
            memset(entry, 0, sizeof(*entry));
            entry->m_Artboard = artboard_name ? strdup(artboard_name) : strdup("");
            entry->m_StateMachines.SetSize(0);
            ++artboard_listener->m_StateMachinesListedCount;
        }
    }

    for (int i = 0; i < DM_MAX_MESSAGE_LOOPS &&
            artboard_listener->m_StateMachinesListedCount < artboard_listener->m_StateMachinesExpectedCount &&
            !artboard_listener->m_ArtboardError; ++i)
    {
        dmRiveCommands::ProcessMessages();
    }

    for (uint32_t i = 0; i < artboards_to_delete.Size(); ++i)
    {
        queue->deleteArtboard(artboards_to_delete[i]);
    }
    if (artboards_to_delete.Size() > 0)
    {
        dmRiveCommands::ProcessMessages();
    }

    out->m_Artboard = queue->instantiateDefaultArtboard(out->m_File, artboard_listener);
    if (out->m_Artboard != RIVE_NULL_HANDLE)
    {
#if defined(DM_RIVE_PLUGIN) || defined(DM_RIVE_VIEWER)
        ClearDefaultViewModelInfo(out->m_DefaultViewModelInfo);
        out->m_HasDefaultViewModelInfo = false;
        artboard_listener->m_DefaultViewModelInfoReceived = false;
        artboard_listener->m_ArtboardError = false;
        queue->requestDefaultViewModelInfo(out->m_Artboard, out->m_File);
        for (int i = 0; i < DM_MAX_MESSAGE_LOOPS && !artboard_listener->m_DefaultViewModelInfoReceived && !artboard_listener->m_ArtboardError; ++i)
        {
            dmRiveCommands::ProcessMessages();
        }
#endif
        out->m_StateMachine = queue->instantiateDefaultStateMachine(out->m_Artboard);
        out->m_ViewModelInstance = queue->instantiateDefaultViewModelInstance(out->m_File, out->m_Artboard);
    }

    dmRiveCommands::ProcessMessages();
    return out;
}

void DestroyFile(RiveFile* file)
{
    if (!file)
    {
        return;
    }

    if (file->m_Path)
    {
        free((void*)file->m_Path);
    }

    ClearStringArray(file->m_Artboards);
    ClearStringArray(file->m_ViewModels);
    ClearArtboardStateMachines(file->m_StateMachinesByArtboard);
    ClearViewModelProperties(file->m_ViewModelProperties);
    ClearViewModelEnums(file->m_ViewModelEnums);
    ClearViewModelInstanceNames(file->m_ViewModelInstanceNames);
    ClearDefaultViewModelInfo(file->m_DefaultViewModelInfo);

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    if (queue)
    {
        if (file->m_StateMachine != RIVE_NULL_HANDLE)
        {
            queue->deleteStateMachine(file->m_StateMachine);
        }
        if (file->m_ViewModelInstance != RIVE_NULL_HANDLE)
        {
            queue->deleteViewModelInstance(file->m_ViewModelInstance);
        }
        if (file->m_Artboard != RIVE_NULL_HANDLE)
        {
            queue->deleteArtboard(file->m_Artboard);
        }
        if (file->m_File != RIVE_NULL_HANDLE)
        {
            queue->deleteFile(file->m_File);
        }
        dmRiveCommands::ProcessMessages();
    }

    delete file->m_ArtboardListener;
    delete file->m_FileListener;

    delete file;
}

void DebugPrintFileState(const RiveFile* file)
{
    if (!file)
    {
        dmLogInfo("RiveFile: <null>");
        return;
    }

    dmLogInfo("RiveFile: path=%s", file->m_Path ? file->m_Path : "<null>");
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


void SetArtboard(RiveFile* file, const char* artboard)
{

}

void SetStatemachine(RiveFile* file, const char* state_machine)
{

}

void SetViewModel(RiveFile* file, const char* view_model)
{

}

void Update(RiveFile* file, float dt)
{

}

bool DrawArtboard(rive::ArtboardInstance* artboard,
                  rive::Renderer* renderer,
                  const DrawArtboardParams& params,
                  rive::Mat2D* out_transform)
{
    if (!artboard || !renderer)
    {
        return false;
    }

    float display_factor = params.m_DisplayFactor;
    if (display_factor == 0.0f)
    {
        display_factor = 1.0f;
    }

    renderer->save();

    rive::AABB bounds = artboard->bounds();

    if (params.m_Fit == rive::Fit::layout)
    {
        artboard->width(params.m_Width / display_factor);
        artboard->height(params.m_Height / display_factor);
    }

    rive::Mat2D renderer_transform = rive::computeAlignment(params.m_Fit,
                                                            params.m_Alignment,
                                                            rive::AABB(0, 0, params.m_Width, params.m_Height),
                                                            bounds,
                                                            display_factor);

    if (out_transform)
    {
        *out_transform = renderer_transform;
    }

    renderer->transform(renderer_transform);
    artboard->draw(renderer);
    renderer->restore();
    return true;
}

}
