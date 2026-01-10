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

#include "file.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dmsdk/dlib/log.h>

#include <common/commands.h>

namespace dmRive
{
namespace FileMeta
{
    namespace
    {
        const int kMaxMessageLoops = 256;

        const char* DuplicateString(const char* value)
        {
            if (!value)
            {
                return 0;
            }
            return strdup(value);
        }

        void ClearStringArray(dmArray<const char*>& array)
        {
            for (uint32_t i = 0; i < array.Size(); ++i)
            {
                free((void*)array[i]);
            }
            array.SetSize(0);
        }

        void FillStringArray(dmArray<const char*>& array, const std::vector<std::string>& values)
        {
            ClearStringArray(array);
            if (!values.empty())
            {
                array.SetCapacity((uint32_t)values.size());
                array.SetSize((uint32_t)values.size());
                for (uint32_t i = 0; i < values.size(); ++i)
                {
                    array[i] = DuplicateString(values[i].c_str());
                }
            }
        }

        void LogNameList(const char* label, const dmArray<const char*>& names)
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
            explicit MetadataListener(RiveFile* file)
            : m_File(file)
            , m_FileLoaded(false)
            , m_FileError(false)
            , m_ArtboardsListed(false)
            , m_ViewModelsListed(false)
            {
            }

            void onFileLoaded(const rive::FileHandle, uint64_t) override
            {
                m_FileLoaded = true;
            }

            void onFileError(const rive::FileHandle, uint64_t, std::string error) override
            {
                dmLogError("Rive file load error: %s", error.c_str());
                m_FileError = true;
            }

            void onArtboardsListed(const rive::FileHandle,
                                   uint64_t,
                                   std::vector<std::string> artboardNames) override
            {
                FillStringArray(m_File->m_Artboards, artboardNames);
                m_ArtboardsListed = true;
            }

            void onViewModelsListed(const rive::FileHandle,
                                    uint64_t,
                                    std::vector<std::string> viewModelNames) override
            {
                FillStringArray(m_File->m_ViewModels, viewModelNames);
                m_ViewModelsListed = true;
            }

            bool m_FileLoaded;
            bool m_FileError;
            bool m_ArtboardsListed;
            bool m_ViewModelsListed;
            RiveFile* m_File;
        };

        class ArtboardMetadataListener : public rive::CommandQueue::ArtboardListener
        {
        public:
            explicit ArtboardMetadataListener(RiveFile* file)
            : m_File(file)
            , m_StateMachinesListed(false)
            , m_ArtboardError(false)
            {
            }

            void onArtboardError(const rive::ArtboardHandle,
                                 uint64_t,
                                 std::string error) override
            {
                dmLogError("Rive artboard error: %s", error.c_str());
                m_ArtboardError = true;
            }

            void onStateMachinesListed(const rive::ArtboardHandle,
                                       uint64_t,
                                       std::vector<std::string> stateMachineNames) override
            {
                FillStringArray(m_File->m_StateMachines, stateMachineNames);
                m_StateMachinesListed = true;
            }

            bool m_StateMachinesListed;
            bool m_ArtboardError;
            RiveFile* m_File;
        };
    }

    RiveFile* LoadFileFromBuffer(const void* buffer, size_t buffer_size, const char* path)
    {
        if (!buffer || buffer_size == 0)
        {
            return 0;
        }

        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
        if (!queue)
        {
            return 0;
        }

        std::vector<uint8_t> bytes((const uint8_t*)buffer, (const uint8_t*)buffer + buffer_size);

        RiveFile* out = new RiveFile;
        out->m_Path = DuplicateString(path);
        out->m_Artboards.SetSize(0);
        out->m_StateMachines.SetSize(0);
        out->m_ViewModels.SetSize(0);
        out->m_File = RIVE_NULL_HANDLE;
        out->m_Artboard = RIVE_NULL_HANDLE;
        out->m_StateMachine = RIVE_NULL_HANDLE;
        out->m_ViewModelInstance = RIVE_NULL_HANDLE;

        MetadataListener listener(out);
        out->m_File = queue->loadFile(bytes, &listener);

        for (int i = 0; i < kMaxMessageLoops && !listener.m_FileLoaded && !listener.m_FileError; ++i)
        {
            dmRiveCommands::ProcessMessages();
        }

        if (listener.m_FileError || !listener.m_FileLoaded)
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
        for (int i = 0; i < kMaxMessageLoops && (!listener.m_ArtboardsListed || !listener.m_ViewModelsListed) && !listener.m_FileError; ++i)
        {
            dmRiveCommands::ProcessMessages();
        }

        ArtboardMetadataListener artboard_listener(out);
        out->m_Artboard = queue->instantiateDefaultArtboard(out->m_File, &artboard_listener);
        if (out->m_Artboard != RIVE_NULL_HANDLE)
        {
            queue->requestStateMachineNames(out->m_Artboard);
            for (int i = 0; i < kMaxMessageLoops && !artboard_listener.m_StateMachinesListed && !artboard_listener.m_ArtboardError; ++i)
            {
                dmRiveCommands::ProcessMessages();
            }
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
        ClearStringArray(file->m_StateMachines);
        ClearStringArray(file->m_ViewModels);

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
        LogNameList("RiveFile: state machines", file->m_StateMachines);
        LogNameList("RiveFile: view models", file->m_ViewModels);

        // if (file->m_Artboards.Size() == 1)
        // {
        //     dmLogInfo("RiveFile: current artboard=%s", file->m_Artboards[0]);
        // }
        // if (file->m_StateMachines.Size() == 1)
        // {
        //     dmLogInfo("RiveFile: current state machine=%s", file->m_StateMachines[0]);
        // }
    }
}
}
