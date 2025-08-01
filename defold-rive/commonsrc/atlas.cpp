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

#include <common/atlas.h>
#include <common/factory.h>
#include <common/types.h>

#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/gamesys/resources/res_textureset.h>

#include <rive/assets/image_asset.hpp>

#if 0
#define DEBUGLOG(...) dmLogWarning("DEBUG: " __VA_ARGS__)
#else
#define DEBUGLOG(...)
#endif

namespace dmRive {

    // struct Region
    // {
    //     float u, v, u2, v2;
    //     float offsetX, offsetY;
    //     uint16_t width, height, degrees;
    // };

    static void CreateRegionFromQuad(const dmGameSystemDDF::TextureSet* texture_set_ddf, const dmGameSystemDDF::TextureSetAnimation* animation_ddf, Region* region)
    {
        const float* tex_coords = (const float*) texture_set_ddf->m_TexCoords.m_Data;

        DEBUGLOG("region: %s", animation_ddf->m_Id);

        uint32_t frame_index = animation_ddf->m_Start;
        const float* tc = &tex_coords[frame_index * 4 * 2];

        DEBUGLOG("  frame_index: %d", frame_index);
        DEBUGLOG("  tc: %.2f, %.2f,  %.2f, %.2f,  %.2f, %.2f,  %.2f, %.2f,", tc[0], tc[1], tc[2], tc[3], tc[4], tc[5], tc[6], tc[7]);

        // From comp_sprite.cpp
        // vertices[0].u = tc[tex_lookup[0] * 2];
        // vertices[0].v = tc[tex_lookup[0] * 2 + 1];
        // vertices[1].u = tc[tex_lookup[1] * 2];
        // vertices[1].v = tc[tex_lookup[1] * 2 + 1];
        // vertices[2].u = tc[tex_lookup[2] * 2];
        // vertices[2].v = tc[tex_lookup[2] * 2 + 1];
        // vertices[3].u = tc[tex_lookup[4] * 2];
        // vertices[3].v = tc[tex_lookup[4] * 2 + 1];

        // From texture_set_ddf.proto
        // For unrotated quads, the order is: [(minU,maxV),(minU,minV),(maxU,minV),(maxU,maxV)]
        // For rotated quads, the order is: [(minU,maxV),(maxU,maxV),(maxU,minV),(minU,minV)]
        // so we compare the Y from vertex 0 and 3
        bool unrotated = tc[0 * 2 + 1] == tc[3 * 2 + 1];

        float minU, minV, maxU, maxV;

        // Since this struct is only used as a placeholder to show which values are needed
        // we only set the ones we care about

        memset(region, 0, sizeof(Region));

        if (unrotated)
        {
            // E.g. tc: 0.00, 0.71,  0.00, 1.00,  0.27, 1.00,  0.27, 0.71,
            //          (minU, minV),(minU, maxV),(maxU,maxV),(maxU,minV)
            minU = tc[0 * 2 + 0];
            minV = tc[0 * 2 + 1];
            maxU = tc[2 * 2 + 0];
            maxV = tc[2 * 2 + 1];

            region->uv1[0] = minU;
            region->uv1[1] = maxV;
            region->uv2[0] = maxU;
            region->uv2[1] = minV;
        }
        else
        {
            // E.g. tc: 0.78, 0.73,  0.84, 0.73,  0.84, 0.64,  0.78, 0.64
            // tc: (minU, maxV), (maxU, maxV), (maxU, minV), (minU, minV)
            minU = tc[3 * 2 + 0];
            minV = tc[3 * 2 + 1];
            maxU = tc[1 * 2 + 0];
            maxV = tc[1 * 2 + 1];

            region->uv1[0] = maxU;
            region->uv1[1] = minV;
            region->uv2[0] = minU;
            region->uv2[1] = maxV;
        }

        DEBUGLOG("  minU/V: %.2f, %.2f  maxU/V: %.2f, %.2f", minU, minV, maxU, maxV);

        region->degrees = unrotated ? 0 : 90; // The uv's are already rotated

        DEBUGLOG("  degrees: %d", region->degrees);

        // We don't support packing yet
        region->offset[0] = 0;
        region->offset[1] = 0;
        region->dimensions[0] = animation_ddf->m_Width;
        region->dimensions[1] = animation_ddf->m_Height;
    }

    Atlas* CreateAtlas(const dmGameSystemDDF::TextureSet* texture_set_ddf)
    {
        dmGameSystemDDF::TextureSetAnimation* animations = texture_set_ddf->m_Animations.m_Data;

        Atlas* atlas = new Atlas;

        uint32_t n_animations = texture_set_ddf->m_Animations.m_Count;
        atlas->m_NameToIndex.SetCapacity(n_animations/2+1, n_animations);
        atlas->m_Regions = new Region[n_animations];

        DEBUGLOG("CreateAtlas, num animations: %d", n_animations);

        for (uint32_t i = 0; i < n_animations; ++i)
        {
            dmGameSystemDDF::TextureSetAnimation* animation_ddf = &animations[i];

            DEBUGLOG("atlas id %s", animation_ddf->m_Id);

            dmhash_t h = dmHashString64(animation_ddf->m_Id);
            atlas->m_NameToIndex.Put(h, i);

            CreateRegionFromQuad(texture_set_ddf, animation_ddf, &atlas->m_Regions[i]);
        }

        return atlas;
    }

    void DestroyAtlas(Atlas* atlas)
    {
        delete[] atlas->m_Regions;
        delete atlas;
    }

    Region* FindAtlasRegion(const Atlas* atlas, dmhash_t name_hash)
    {
        if (atlas == 0)
        {
            return 0;
        }
        const uint32_t* pindex = atlas->m_NameToIndex.Get(name_hash);
        if (!pindex)
            return 0;
        return &atlas->m_Regions[*pindex];
    }

    void ConvertRegionToAtlasUV(const Region* region, uint32_t count, const float* uvs, rive::Vec2D* outuvs)
    {
        bool rotate   = region->degrees == 90;
        float offsetu = region->offset[0];
        float offsetv = region->offset[1];

        // Width of the image in atlas space
        float width  = region->uv2[0] - region->uv1[0];
        float height = region->uv1[1] - region->uv2[1];

        if (rotate)
        {
            width    = region->uv1[0] - region->uv2[0];
            height   = region->uv2[1] - region->uv1[1];
            offsetu += region->uv1[0];
            offsetv += region->uv1[1];
        }
        else
        {
            offsetu += region->uv1[0];
            offsetv += region->uv2[1];
        }

        for (int i = 0; i < count; ++i)
        {
            float u = uvs[i*2+0];
            float v = uvs[i*2+1];

            // Rotate: R90(x,y) -> (-y,x)
            float ru = rotate ? -v : u;
            float rv = rotate ? u : v;

            // Scale the coordinate by the image size in atlas space
            float su = width * ru;
            float sv = height - height * rv;

            // Offset the uv to the correct place in the atlas
            float outu = offsetu + su;
            float outv = offsetv + sv;

            // dmLogInfo("  uv: %f, %f w/h: %f, %f", outu, outv, width, height);

            outuvs[i].x = outu;
            outuvs[i].y = outv;
        }
    }

    rive::rcp<rive::RenderImage> LoadImageFromMemory(HRenderContext context, void* resource, uint32_t resource_size)
    {
        return CreateRiveRenderImage(context, resource, resource_size);
    }

    rive::rcp<rive::RenderImage> LoadImageFromFactory(dmResource::HFactory factory, HRenderContext context, const char* path)
    {
        if (!factory)
        {
            dmLogError("No factory provided!");
            return rive::rcp<rive::RenderImage>();
        }

        char path_buffer[2048];
        uint32_t path_length = 0;
        if (path[0] != '/')
            path_buffer[path_length++] = '/';
        path_length += dmStrlCpy(&path_buffer[path_length], path, sizeof(path_buffer)-path_length);

        void* resource;
        uint32_t resource_size;
        dmResource::Result r = dmResource::GetRaw(factory, path_buffer, &resource, &resource_size);
        if (dmResource::RESULT_OK != r)
        {
            dmLogError("Error getting file '%s': %d", path_buffer, r);
            return rive::rcp<rive::RenderImage>();
        }

        return LoadImageFromMemory(context, resource, resource_size);
    }

    AtlasNameResolver::AtlasNameResolver(dmResource::HFactory factory, HRenderContext context)
    : m_Factory(factory)
    , m_RiveRenderContext(context)
    {
    }

    bool AtlasNameResolver::loadContents(rive::FileAsset& _asset, rive::Span<const uint8_t> inBandBytes, rive::Factory* factory)
    {
        DEBUGLOG("loadContents");

        if (m_Assets.Full())
        {
            m_Assets.OffsetCapacity(8);
        }
        m_Assets.Push(&_asset);

        bool out_of_band = inBandBytes.size() == 0;
        if (_asset.is<rive::ImageAsset>())
        {
            rive::ImageAsset* asset = _asset.as<rive::ImageAsset>();
            const std::string& name = asset->name();

            rive::rcp<rive::RenderImage> image;

            if (out_of_band)
            {
                image = LoadImageFromFactory(m_Factory, m_RiveRenderContext, name.c_str());
            }

            if (!image)
            {
                image = CreateRiveRenderImage(m_RiveRenderContext, (void*) inBandBytes.data(), inBandBytes.size());
                DEBUGLOG("  In band asset: file: '%s' data: %u bytes", name.c_str(), (uint32_t)inBandBytes.size());
            }

            asset->renderImage(image);
            return (bool)image;
        }

        return false;
    }


} // namespace dmRive
