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
#include "file_meta.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dmsdk/dlib/log.h>

#include <common/commands.h>

namespace dmRive
{

static const int DM_MAX_MESSAGE_LOOPS = 256;

static void UpdateArtboardBounds(RiveFile* file);

RiveFile* LoadFileFromBuffer(const void* buffer, size_t buffer_size, const char* path)
{
    if (!buffer || buffer_size == 0)
    {
        return 0;
    }

    std::vector<uint8_t> bytes((const uint8_t*)buffer, (const uint8_t*)buffer + buffer_size);

    RiveFile* out = new RiveFile;
    out->m_Path = strdup(path);
    out->m_File = RIVE_NULL_HANDLE;
    out->m_Artboard = RIVE_NULL_HANDLE;
    out->m_StateMachine = RIVE_NULL_HANDLE;
    out->m_ViewModelInstance = RIVE_NULL_HANDLE;
    out->m_Bounds = rive::AABB();
    out->m_FileListener = new MetadataListener(out);

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

#if defined(DM_RIVE_FILE_META_DATA)
    InitFileMetaData(out);
    RequestMetaData(out, queue);
#endif

#if defined(DM_RIVE_FILE_META_DATA)
    out->m_Artboard = InstantiateDefaultArtboardWithMeta(out, queue);
#else
    out->m_Artboard = queue->instantiateDefaultArtboard(out->m_File, 0);
#endif

    if (out->m_Artboard != RIVE_NULL_HANDLE)
    {
        out->m_StateMachine = queue->instantiateDefaultStateMachine(out->m_Artboard);
        out->m_ViewModelInstance = queue->instantiateDefaultViewModelInstance(out->m_File, out->m_Artboard);
    }

    dmRiveCommands::ProcessMessages();
    UpdateArtboardBounds(out);
    return out;
}

void DestroyFile(RiveFile* file)
{
    if (!file)
    {
        return;
    }


#if defined(DM_RIVE_FILE_META_DATA)
    DestroyFileMetaData(file);
#endif

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

    if (file->m_Path)
    {
        free((void*)file->m_Path);
    }

    delete file->m_FileListener;

    delete file;
}

static void UpdateArtboardBounds(RiveFile* file)
{
    if (!file)
    {
        return;
    }

    rive::AABB bounds;
    if (dmRiveCommands::GetBounds(file->m_Artboard, &bounds))
    {
        file->m_Bounds = bounds;
    }
    else
    {
        file->m_Bounds = rive::AABB();
    }
}

void DebugPrintFileState(const RiveFile* file)
{
    if (!file)
    {
        dmLogInfo("RiveFile: <null>");
        return;
    }

    dmLogInfo("RiveFile: path=%s", file->m_Path ? file->m_Path : "<null>");

#if defined(DM_RIVE_FILE_META_DATA)
    DebugPrintFileMetaData(file);
#endif
}


void SetArtboard(RiveFile* file, const char* artboard)
{
    if (!file)
    {
        return;
    }

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    if (!queue)
    {
        return;
    }

    if (file->m_StateMachine != RIVE_NULL_HANDLE)
    {
        queue->deleteStateMachine(file->m_StateMachine);
        file->m_StateMachine = RIVE_NULL_HANDLE;
    }
    if (file->m_ViewModelInstance != RIVE_NULL_HANDLE)
    {
        queue->deleteViewModelInstance(file->m_ViewModelInstance);
        file->m_ViewModelInstance = RIVE_NULL_HANDLE;
    }
    if (file->m_Artboard != RIVE_NULL_HANDLE)
    {
        queue->deleteArtboard(file->m_Artboard);
        file->m_Artboard = RIVE_NULL_HANDLE;
    }

#if defined(DM_RIVE_FILE_META_DATA)
    if (artboard && artboard[0] != 0)
    {
        file->m_Artboard = InstantiateArtboardNamedWithMeta(file, artboard, queue);
    }
    else
    {
        file->m_Artboard = InstantiateDefaultArtboardWithMeta(file, queue);
    }
#else
    if (artboard && artboard[0] != 0)
    {
        file->m_Artboard = queue->instantiateArtboardNamed(file->m_File, artboard);
    }
    else
    {
        file->m_Artboard = queue->instantiateDefaultArtboard(file->m_File);
    }
#endif

    if (file->m_Artboard != RIVE_NULL_HANDLE)
    {
        file->m_StateMachine = queue->instantiateDefaultStateMachine(file->m_Artboard);
        file->m_ViewModelInstance = queue->instantiateDefaultViewModelInstance(file->m_File, file->m_Artboard);
    }

    dmRiveCommands::ProcessMessages();
    UpdateArtboardBounds(file);
}

void SetStatemachine(RiveFile* file, const char* state_machine)
{
    if (!file)
    {
        return;
    }

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    if (!queue)
    {
        return;
    }

    if (file->m_StateMachine != RIVE_NULL_HANDLE)
    {
        queue->deleteStateMachine(file->m_StateMachine);
        file->m_StateMachine = RIVE_NULL_HANDLE;
    }

    if (file->m_Artboard == RIVE_NULL_HANDLE)
    {
        dmRiveCommands::ProcessMessages();
        return;
    }

    if (state_machine && state_machine[0] != 0)
    {
        file->m_StateMachine = queue->instantiateStateMachineNamed(file->m_Artboard, state_machine);
        if (file->m_StateMachine == RIVE_NULL_HANDLE)
        {
            dmLogWarning("Could not find state machine with name '%s'", state_machine);
        }
    }

    if (file->m_StateMachine == RIVE_NULL_HANDLE)
    {
        file->m_StateMachine = queue->instantiateDefaultStateMachine(file->m_Artboard);
    }

    dmRiveCommands::ProcessMessages();
}

void SetViewModel(RiveFile* file, const char* view_model)
{

}

void Update(RiveFile* file, float dt)
{
    if (!file)
    {
        return;
    }

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    if (!queue)
    {
        return;
    }

    if (file->m_StateMachine != RIVE_NULL_HANDLE)
    {
        queue->advanceStateMachine(file->m_StateMachine, dt);
    }

    dmRiveCommands::ProcessMessages();
    UpdateArtboardBounds(file);
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
