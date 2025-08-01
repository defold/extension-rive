// Copyright 2020 The Defold Foundation
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

#ifndef DM_RIVE_ATLAS_H
#define DM_RIVE_ATLAS_H

#include <stdint.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/hashtable.h>

#include <rive/renderer.hpp>
#include <rive/assets/file_asset.hpp>
#include <rive/file_asset_loader.hpp>

#include "defold/renderer.h"

namespace dmGameSystemDDF {
    struct TextureSet;
}

namespace dmRive {
    struct Region
    {
        float uv1[2];
        float uv2[2];
        float offset[2];     // x, y
        float dimensions[2]; // width, height
        uint16_t degrees;    // 0, 90, 180, 270
    };

    struct Atlas
    {
        dmHashTable64<uint32_t> m_NameToIndex;
        Region*                 m_Regions;
    };

    class AtlasNameResolver : public rive::FileAssetLoader {
    public:
        AtlasNameResolver(dmResource::HFactory factory, HRenderContext context);

        bool loadContents(rive::FileAsset& asset, rive::Span<const uint8_t> inBandBytes, rive::Factory* factory);

        dmArray<rive::FileAsset*>& GetAssets() { return m_Assets; }

    private:
        HRenderContext       m_RiveRenderContext;
        dmResource::HFactory m_Factory;

        dmArray<rive::FileAsset*> m_Assets;
    };

    // Gets the resource as a Raw payload, and loads an image from it
    rive::rcp<rive::RenderImage> LoadImageFromFactory(dmResource::HFactory factory, HRenderContext context, const char* name);
    rive::rcp<rive::RenderImage> LoadImageFromMemory(HRenderContext context, void* resource, uint32_t resource_size);

    Atlas*      CreateAtlas(const dmGameSystemDDF::TextureSet* texture_set_ddf);
    void        DestroyAtlas(Atlas* atlas);
    Region*     FindAtlasRegion(const Atlas* atlas, dmhash_t name_hash);
    void        ConvertRegionToAtlasUV(const Region* region, uint32_t count, const float* uvs, rive::Vec2D* outuvs);

} // namespace dmRive

#endif
