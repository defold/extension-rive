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

#ifndef DM_RIVE_COMMONSRC_FILE_H
#define DM_RIVE_COMMONSRC_FILE_H

#include <stddef.h>
#include <stdint.h>

#include <dmsdk/dlib/array.h>

#include <rive/command_queue.hpp>
#include <rive/artboard.hpp>
#include <rive/data_bind/data_values/data_type.hpp>
#include <rive/renderer.hpp>

#if defined(DM_RIVE_PLUGIN) || defined(DM_RIVE_VIEWER)
    // Used for retrieving edit-time related information
    #define DM_RIVE_FILE_META_DATA
#endif

namespace dmRive
{
    class MetadataListener;

#if defined(DM_RIVE_FILE_META_DATA)
    class ArtboardMetadataListener;

    struct ViewModelProperty
    {
        const char*      m_ViewModel;
        const char*      m_Name;
        rive::DataType   m_Type;
        const char*      m_MetaData;
        const char*      m_Value;
    };

    struct ViewModelEnum
    {
        const char*          m_Name;
        dmArray<const char*> m_Enumerants;
    };

    struct ViewModelInstanceNames
    {
        const char*          m_ViewModel;
        dmArray<const char*> m_Instances;
    };

    struct ArtboardStateMachines
    {
        const char*          m_Artboard;
        dmArray<const char*> m_StateMachines;
    };

    struct DefaultViewModelInfo
    {
        const char* m_ViewModel;
        const char* m_Instance;
    };
#endif // DM_RIVE_FILE_META_DATA

    struct RiveFile
    {
        const char*                  m_Path;

        rive::FileHandle             m_File;
        rive::ArtboardHandle         m_Artboard;
        rive::StateMachineHandle     m_StateMachine;
        rive::ViewModelInstanceHandle m_ViewModelInstance;
        rive::AABB                     m_Bounds;

        MetadataListener*                     m_FileListener;

#if defined(DM_RIVE_FILE_META_DATA)
        ArtboardMetadataListener*             m_ArtboardListener;
        dmArray<const char*>                    m_Artboards;
        dmArray<const char*>                    m_ViewModels;
        dmArray<struct ArtboardStateMachines>   m_StateMachinesByArtboard;
        dmArray<struct ViewModelProperty>       m_ViewModelProperties;
        dmArray<struct ViewModelEnum>           m_ViewModelEnums;
        dmArray<struct ViewModelInstanceNames>  m_ViewModelInstanceNames;
        DefaultViewModelInfo                    m_DefaultViewModelInfo;
        bool                                    m_HasDefaultViewModelInfo;
#endif // DM_RIVE_FILE_META_DATA
    };

    RiveFile* LoadFileFromBuffer(const void* buffer, size_t buffer_size, const char* path);
    void      DestroyFile(RiveFile* file);
    void      DebugPrintFileState(const RiveFile* file);

    void Update(RiveFile* file, float dt);

    void SetArtboard(RiveFile* file, const char* artboard);
    void SetStatemachine(RiveFile* file, const char* state_machine);
    void SetViewModel(RiveFile* file, const char* view_model);

    struct DrawArtboardParams
    {
        rive::Fit       m_Fit;
        rive::Alignment m_Alignment;
        uint32_t        m_Width;
        uint32_t        m_Height;
        float           m_DisplayFactor;
    };

    bool DrawArtboard(rive::ArtboardInstance* artboard,
                      rive::Renderer* renderer,
                      const DrawArtboardParams& params,
                      rive::Mat2D* out_transform);

    //jobject GetTexture(RiveFile* file);
} // namespace dmRive

#endif // DM_RIVE_COMMONSRC_FILE_H
