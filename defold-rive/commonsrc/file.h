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

#include <dmsdk/dlib/array.h>

#include <rive/command_queue.hpp>

namespace dmRive
{
namespace FileMeta
{
    struct RiveFile
    {
        const char*                  m_Path;
        dmArray<const char*>         m_Artboards;
        dmArray<const char*>         m_StateMachines;
        dmArray<const char*>         m_ViewModels;
        rive::FileHandle             m_File;
        rive::ArtboardHandle         m_Artboard;
        rive::StateMachineHandle     m_StateMachine;
        rive::ViewModelInstanceHandle m_ViewModelInstance;
    };

    RiveFile* LoadFileFromBuffer(const void* buffer, size_t buffer_size, const char* path);
    void      DestroyFile(RiveFile* file);
    void      DebugPrintFileState(const RiveFile* file);
} // namespace FileMeta
} // namespace dmRive

#endif // DM_RIVE_COMMONSRC_FILE_H
