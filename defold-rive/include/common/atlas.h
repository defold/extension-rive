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
#include <dmsdk/dlib/hashtable.h>

#include <rive/renderer.hpp>
#include <rive/assets/file_asset.hpp>
#include <rive/file_asset_resolver.hpp>

namespace dmGameSystemDDF {
    class TextureSet;
}

namespace dmRive {

struct Region;

// class AtlasResolver : public rive::FileAssetResolver {
// private:
//     dmGameSystemDDF::TextureSet* m_TextureSet;
//     dmHashTable64<uint32_t> m_NameToIndex;
//     Region* m_Regions;
//     uint32_t m_NumRegions;

// public:
//     AtlasResolver(dmGameSystemDDF::TextureSet* texture_set_ddf);
//     void loadContents(rive::FileAsset& asset) override;
// };

class AtlasNameResolver : public rive::FileAssetResolver {
public:
    AtlasNameResolver();
    void loadContents(rive::FileAsset& asset) override;
};

Region* CreateRegions(const dmGameSystemDDF::TextureSet* texture_set_ddf, dmHashTable64<uint32_t>& name_to_index);
Region* FindAtlasRegion(const dmHashTable64<uint32_t>& name_to_index, Region* regions, const char* name);

} // namespace dmRive

#endif
