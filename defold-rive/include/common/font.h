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

#ifndef DM_RIVE_FONT_H
#define DM_RIVE_FONT_H

#include <stdint.h>
#include <dmsdk/resource/resource.h>
#include <rive/refcnt.hpp>

namespace rive
{
    class Font;
}

#include "defold/renderer.h"

namespace dmRive {

    // Gets the resource as a Raw payload, and loads a font from it
    rive::rcp<rive::Font> LoadFontFromFactory(dmResource::HFactory factory, HRenderContext context, const char* name);
    rive::rcp<rive::Font> LoadFontFromMemory(HRenderContext context, void* resource, uint32_t resource_size);

} // namespace dmRive

#endif