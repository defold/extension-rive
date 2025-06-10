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

#ifndef DM_RES_RIVE_DATA_H
#define DM_RES_RIVE_DATA_H

#include <stdint.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hash.h>
#include "defold/renderer.h"

namespace rive
{
    class File;
    class FileAsset;
    class ArtboardInstance;
}

namespace dmRive
{
    struct RiveBone;

    struct RiveArtboardIdList
    {
        dmhash_t                                m_ArtboardNameHash;
        dmArray<dmhash_t>                       m_LinearAnimations;
        dmArray<dmhash_t>                       m_StateMachines;
    };

    struct RiveSceneData
    {
        rive::File*                             m_File;
        HRenderContext                          m_RiveRenderContext;
        std::unique_ptr<rive::ArtboardInstance> m_ArtboardDefault;
        dmArray<RiveArtboardIdList*>            m_ArtboardIdLists;
        dmArray<rive::FileAsset*>               m_FileAssets;       // For runtime swapping
    };

    dmResource::Result ResRiveDataSetAssetFromMemory(RiveSceneData* resource, const char* asset_name, void* payload, uint32_t payload_size);
    dmResource::Result ResRiveDataSetAsset(dmResource::HFactory factory, RiveSceneData* resource, const char* asset_name, const char* path);
}

#endif // DM_RES_RIVE_DATA_H
