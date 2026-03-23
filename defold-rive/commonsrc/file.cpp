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

static bool FileLoadFinished(void* user_data)
{
    MetadataListener* listener = (MetadataListener*)user_data;
    return listener->m_FileLoaded || listener->m_FileError;
}

static void UpdateArtboardBounds(RiveFile* file);

#if defined(DM_RIVE_FILE_META_DATA)
static void SetSelectedArtboardName(RiveFile* file, const char* artboard_name)
{
    free((void*)file->m_SelectedArtboardName);
    file->m_SelectedArtboardName = 0;

    if (artboard_name && artboard_name[0] != 0)
    {
        file->m_SelectedArtboardName = strdup(artboard_name);
    }
}
#endif

static const char* ResolveArtboardName(const RiveFile* file, const char* artboard_name)
{
#if defined(DM_RIVE_FILE_META_DATA)
    if (artboard_name && artboard_name[0] != 0)
    {
        return artboard_name;
    }
    if (file->m_Artboards.Size() > 0)
    {
        return file->m_Artboards[0];
    }
#else
    (void)file;
    (void)artboard_name;
#endif
    return 0;
}

static bool ArtboardHasAnyStateMachines(const RiveFile* file, const char* artboard_name)
{
#if defined(DM_RIVE_FILE_META_DATA)
    if (file->m_StateMachinesByArtboard.Size() == 0)
    {
        return true;
    }

    const char* resolved_artboard = ResolveArtboardName(file, artboard_name);
    if (resolved_artboard == 0)
    {
        return true;
    }

    for (uint32_t i = 0; i < file->m_StateMachinesByArtboard.Size(); ++i)
    {
        const ArtboardStateMachines& entry = file->m_StateMachinesByArtboard[i];
        if (entry.m_Artboard && strcmp(entry.m_Artboard, resolved_artboard) == 0)
        {
            return entry.m_StateMachines.Size() > 0;
        }
    }
#else
    (void)file;
    (void)artboard_name;
#endif
    return true;
}

static bool ShouldInstantiateDefaultStateMachine(const RiveFile* file, const char* artboard_name)
{
#if defined(DM_RIVE_FILE_META_DATA)
    if (!ArtboardHasAnyStateMachines(file, artboard_name))
    {
        return false;
    }
    if (file->m_ViewModels.Size() == 0)
    {
        return true;
    }
    return file->m_HasDefaultViewModelInfo;
#else
    return true;
#endif
}

static bool ShouldInstantiateDefaultViewModelInstance(const RiveFile* file)
{
#if defined(DM_RIVE_FILE_META_DATA)
    return file->m_HasDefaultViewModelInfo;
#else
    return true;
#endif
}

static void BindViewModelInstance(RiveFile* file, rive::rcp<rive::CommandQueue> queue)
{
    if (!file || !queue)
    {
        return;
    }

    if (file->m_StateMachine == RIVE_NULL_HANDLE || file->m_ViewModelInstance == RIVE_NULL_HANDLE)
    {
        return;
    }

    queue->bindViewModelInstance(file->m_StateMachine, file->m_ViewModelInstance);
}

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
    out->m_Fit = rive::Fit::contain;
    out->m_Alignment = rive::Alignment::center;
    out->m_Bounds = rive::AABB();
    out->m_FileListener = new MetadataListener(out);

#if defined(DM_RIVE_FILE_META_DATA)
    // Initialize metadata storage up-front so early-failure cleanup does not
    // free uninitialized pointers.
    InitFileMetaData(out);
#endif

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();

    MetadataListener* listener = out->m_FileListener;
    out->m_File = queue->loadFile(bytes, listener);

    dmRiveCommands::WaitUntil(FileLoadFinished, listener, 5000000);

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
    RequestMetaData(out, queue);
#endif

#if defined(DM_RIVE_FILE_META_DATA)
    out->m_Artboard = InstantiateDefaultArtboardWithMeta(out, queue);
#else
    out->m_Artboard = queue->instantiateDefaultArtboard(out->m_File, 0);
#endif

    if (out->m_Artboard != RIVE_NULL_HANDLE)
    {
#if defined(DM_RIVE_FILE_META_DATA)
        SetSelectedArtboardName(out, ResolveArtboardName(out, 0));
        if (ShouldInstantiateDefaultStateMachine(out, out->m_SelectedArtboardName))
#else
        if (ShouldInstantiateDefaultStateMachine(out, 0))
#endif
        {
            out->m_StateMachine = queue->instantiateDefaultStateMachine(out->m_Artboard);
        }
        if (out->m_StateMachine != RIVE_NULL_HANDLE &&
            ShouldInstantiateDefaultViewModelInstance(out))
        {
            out->m_ViewModelInstance = queue->instantiateDefaultViewModelInstance(out->m_File, out->m_Artboard);
        }
        BindViewModelInstance(out, queue);
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
#if defined(DM_RIVE_FILE_META_DATA)
        SetSelectedArtboardName(file, ResolveArtboardName(file, artboard));
        if (ShouldInstantiateDefaultStateMachine(file, file->m_SelectedArtboardName))
#else
        if (ShouldInstantiateDefaultStateMachine(file, artboard))
#endif
        {
            file->m_StateMachine = queue->instantiateDefaultStateMachine(file->m_Artboard);
        }
        if (file->m_StateMachine != RIVE_NULL_HANDLE &&
            ShouldInstantiateDefaultViewModelInstance(file))
        {
            file->m_ViewModelInstance = queue->instantiateDefaultViewModelInstance(file->m_File, file->m_Artboard);
        }
        BindViewModelInstance(file, queue);
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
#if defined(DM_RIVE_FILE_META_DATA)
        if (ShouldInstantiateDefaultStateMachine(file, file->m_SelectedArtboardName))
#else
        if (ShouldInstantiateDefaultStateMachine(file, 0))
#endif
        {
            file->m_StateMachine = queue->instantiateDefaultStateMachine(file->m_Artboard);
        }
    }

    BindViewModelInstance(file, queue);
    dmRiveCommands::ProcessMessages();
}

void SetFitAlignment(RiveFile* file, rive::Fit fit, rive::Alignment alignment)
{
    if (!file)
    {
        return;
    }

    file->m_Fit = fit;
    file->m_Alignment = alignment;
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

rive::Mat2D CalcTransformRive(rive::ArtboardInstance* artboard,
                              rive::Fit fit,
                              rive::Alignment alignment,
                              uint32_t width,
                              uint32_t height,
                              float display_factor)
{
    if (!artboard)
    {
        return rive::Mat2D();
    }

    if (display_factor == 0.0f)
    {
        display_factor = 1.0f;
    }

    rive::AABB bounds = artboard->bounds();

    if (fit == rive::Fit::layout)
    {
        artboard->width(width / display_factor);
        artboard->height(height / display_factor);
        bounds = artboard->bounds();
    }

    return rive::computeAlignment(fit,
                                  alignment,
                                  rive::AABB(0, 0, width, height),
                                  bounds,
                                  display_factor);
}

rive::Mat2D CalcTransformGame(rive::ArtboardInstance* artboard,
                              const rive::Mat2D& view_transform,
                              const rive::Mat2D& game_transform,
                              float display_factor,
                              float window_height)
{
    if (!artboard)
    {
        return rive::Mat2D();
    }

    if (display_factor == 0.0f)
    {
        display_factor = 1.0f;
    }

    rive::AABB bounds = artboard->bounds();
    rive::Mat2D center_adjustment = rive::Mat2D::fromTranslate(-bounds.width() / 2.0f, -bounds.height() / 2.0f);
    rive::Mat2D scale_dpi = rive::Mat2D::fromScale(1, -1);
    rive::Mat2D invert_adjustment = rive::Mat2D::fromScaleAndTranslation(display_factor, -display_factor, 0, window_height);
    return invert_adjustment * view_transform * game_transform * scale_dpi * center_adjustment;
}

bool DrawArtboard(rive::ArtboardInstance* artboard,
                  rive::Renderer* renderer,
                  const rive::Mat2D& renderer_transform)
{
    if (!artboard || !renderer)
    {
        return false;
    }

    renderer->save();
    renderer->transform(renderer_transform);
    artboard->draw(renderer);
    renderer->restore();
    return true;
}

}
