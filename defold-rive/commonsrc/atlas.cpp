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

#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/gamesys/resources/res_textureset.h>

#include <rive/assets/image_asset.hpp>

#if 1
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

// static Region* CreateRegionsFromQuads(const dmGameSystemDDF::TextureSet* texture_set_ddf)
// {
//     const float* tex_coords = (const float*) texture_set_ddf->m_TexCoords.m_Data;
//     uint32_t n_animations = texture_set_ddf->m_Animations.m_Count;
//     dmGameSystemDDF::TextureSetAnimation* animations = texture_set_ddf->m_Animations.m_Data;

//     Region* regions = new Region[n_animations];
//     for (uint32_t i = 0; i < n_animations; ++i)
//     {
//             dmGameSystemDDF::TextureSetAnimation* animation_ddf = &animations[i];

//             DEBUGLOG("region: %d %s", i, animation_ddf->m_Id);

// // required string id              = 1;
// // required uint32 width           = 2;
// // required uint32 height          = 3;
// // required uint32 start           = 4;
// // required uint32 end             = 5;
// // optional uint32 fps             = 6 [default = 30];
// // optional Playback playback      = 7 [default = PLAYBACK_ONCE_FORWARD];
// // optional uint32 flip_horizontal = 8 [default = 0];
// // optional uint32 flip_vertical   = 9 [default = 0];
// // optional uint32 is_animation    = 10 [default = 0]; // Deprecated


//             uint32_t frame_index = animation_ddf->m_Start;
//             const float* tc = &tex_coords[frame_index * 4 * 2];

//             DEBUGLOG("  frame_index: %d", frame_index);
//             DEBUGLOG("  tc: %.2f, %.2f,  %.2f, %.2f,  %.2f, %.2f,  %.2f, %.2f,", tc[0], tc[1], tc[2], tc[3], tc[4], tc[5], tc[6], tc[7]);

//             // From comp_sprite.cpp
//             // vertices[0].u = tc[tex_lookup[0] * 2];
//             // vertices[0].v = tc[tex_lookup[0] * 2 + 1];
//             // vertices[1].u = tc[tex_lookup[1] * 2];
//             // vertices[1].v = tc[tex_lookup[1] * 2 + 1];
//             // vertices[2].u = tc[tex_lookup[2] * 2];
//             // vertices[2].v = tc[tex_lookup[2] * 2 + 1];
//             // vertices[3].u = tc[tex_lookup[4] * 2];
//             // vertices[3].v = tc[tex_lookup[4] * 2 + 1];

//             // From texture_set_ddf.proto
//             // For unrotated quads, the order is: [(minU,maxV),(minU,minV),(maxU,minV),(maxU,maxV)]
//             // For rotated quads, the order is: [(minU,maxV),(maxU,maxV),(maxU,minV),(minU,minV)]
//             // so we compare the Y from vertex 0 and 3
//             bool unrotated = tc[0 * 2 + 1] == tc[3 * 2 + 1];

//             float minU, minV, maxU, maxV;

//             // Since this struct is only used as a placeholder to show which values are needed
//             // we only set the ones we care about

//             Region* region = &regions[i];
//             memset(region, 0, sizeof(Region));

//             if (unrotated)
//             {
//                 // E.g. tc: 0.00, 0.71,  0.00, 1.00,  0.27, 1.00,  0.27, 0.71,
//                 //          (minU, minV),(minU, maxV),(maxU,maxV),(maxU,minV)
//                 minU = tc[0 * 2 + 0];
//                 minV = tc[0 * 2 + 1];
//                 maxU = tc[2 * 2 + 0];
//                 maxV = tc[2 * 2 + 1];

//                 region->u   = minU;
//                 region->v   = maxV;
//                 region->u2  = maxU;
//                 region->v2  = minV;
//             }
//             else
//             {
//                 // E.g. tc: 0.78, 0.73,  0.84, 0.73,  0.84, 0.64,  0.78, 0.64
//                 // tc: (minU, maxV), (maxU, maxV), (maxU, minV), (minU, minV)
//                 minU = tc[3 * 2 + 0];
//                 minV = tc[3 * 2 + 1];
//                 maxU = tc[1 * 2 + 0];
//                 maxV = tc[1 * 2 + 1];

//                 region->u   = maxU;
//                 region->v   = minV;
//                 region->u2  = minU;
//                 region->v2  = maxV;
//             }

//             DEBUGLOG("  minU/V: %.2f, %.2f  maxU/V: %.2f, %.2f", minU, minV, maxU, maxV);

//             region->degrees = unrotated ? 0 : 90; // The uv's are already rotated

//             DEBUGLOG("  degrees: %d", region->degrees);

//             // We don't support packing yet
//             region->offsetX = 0;
//             region->offsetY = 0;
//             region->width = animation_ddf->m_Width;
//             region->height = animation_ddf->m_Height;
//     }

//     return regions;
// }


// // Create an array or regions given the atlas. Maps 1:1 with the animation count
// Region* CreateRegions(const dmGameSystemDDF::TextureSet* texture_set_ddf, dmHashTable64<uint32_t>& name_to_index)
// {
//     uint32_t n_animations = texture_set_ddf->m_Animations.m_Count;
//     name_to_index.SetCapacity(n_animations/2+1, n_animations);
//     for (uint32_t i = 0; i < n_animations; ++i)
//     {
//         dmhash_t h = dmHashString64(texture_set_ddf->m_Animations[i].m_Id);
//         name_to_index.Put(h, i);
//     }
//     return CreateRegionsFromQuads(texture_set_ddf);
// }

// Region* FindAtlasRegion(const dmHashTable64<uint32_t>& name_to_index, Region* regions, const char* name)
// {
//     dmhash_t name_hash = dmHashString64(name);
//     const uint32_t* anim_index = name_to_index.Get(name_hash);
//     if (!anim_index)
//         return 0;
//     return &regions[*anim_index];
// }


AtlasNameResolver::AtlasNameResolver()
{
}

void AtlasNameResolver::loadContents(rive::FileAsset& _asset)
{
    if (_asset.is<rive::ImageAsset>()) {
        rive::ImageAsset* asset = _asset.as<rive::ImageAsset>();
        const std::string& name = asset->name();
        dmhash_t name_hash = dmHashString64(name.c_str());
        asset->renderImage(std::make_unique<DefoldRenderImage>(name_hash));
    }
}

// AtlasResolver::AtlasResolver(dmGameSystemDDF::TextureSet* texture_set_ddf)
// : m_TextureSet(texture_set_ddf)
// {
//     m_Regions = CreateRegions(texture_set_ddf, m_NameToIndex);
// }

// void AtlasResolver::loadContents(rive::FileAsset& _asset) {
//     if (_asset.is<rive::ImageAsset>()) {
//         rive::ImageAsset* asset = _asset.as<rive::ImageAsset>();

//         const std::string& name = asset->name();

//         Region* region = FindAtlasRegion(m_NameToIndex, m_Regions, name.c_str());
//         if (!region)
//         {
//             dmLogError("Failed to find asset '%s' in atlas", name.c_str());
//             return;
//         }

//         dmhash_t name_hash = dmHashString64(name.c_str());
//         asset->renderImage(std::make_unique<DefoldRenderImage>(name_hash));


//         //SampleAtlasLocation location;

//         // // Find which location this image got packed into.
//         // if (m_packer->find(*imageAsset, &location)) {
//         //     // Determine if we've already loaded the a render image.
//         //     auto sharedItr = m_sharedImageResources.find(location.atlasIndex);

//         //     rive::rcp<rive::SokolRenderImageResource> imageResource;
//         //     if (sharedItr != m_sharedImageResources.end()) {
//         //         imageResource = sharedItr->second;
//         //     } else {
//         //         auto atlas = m_packer->atlas(location.atlasIndex);
//         //         imageResource = rive::rcp<rive::SokolRenderImageResource>(
//         //             new SokolRenderImageResource(atlas->pixels().data(),
//         //                                          atlas->width(),
//         //                                          atlas->height()));
//         //         m_sharedImageResources[location.atlasIndex] = imageResource;
//         //     }

//         //     // Make a render image from the atlas if we haven't yet. Our Factory
//         //     // doesn't have an abstraction for creating a RenderImage from a
//         //     // decoded set of pixels, we should either add that or live with/be
//         //     // ok with the fact that most people writing a resolver doing
//         //     // something like this will likely also have a custom/specific
//         //     // renderer (and hence will know which RenderImage they need to
//         //     // make).

//         //     imageAsset->renderImage(std::make_unique<SokolRenderImage>(imageResource,
//         //                                                                location.width,
//         //                                                                location.height,
//         //                                                                location.transform));
//         // }
//     }
// }


} // namespace dmRive
