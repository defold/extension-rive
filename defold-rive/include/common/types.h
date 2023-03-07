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

#ifndef DM_RIVE_TYPES_H
#define DM_RIVE_TYPES_H

#include <dmsdk/dlib/hash.h>
#include <rive/renderer.hpp>

namespace dmRive {

class DefoldRenderImage : public rive::RenderImage {
private:
    

public:
    dmhash_t m_NameHash;
    DefoldRenderImage(dmhash_t name_hash);
};


} // namespace dmRive

#endif
