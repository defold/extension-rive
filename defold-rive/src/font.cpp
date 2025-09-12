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

#include <common/font.h>
#include <common/factory.h>

#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/log.h>

#include <rive/text_engine.hpp>

namespace dmRive {

    rive::rcp<rive::Font> LoadFontFromMemory(HRenderContext context, void* resource, uint32_t resource_size)
    {
        rive::Factory* rive_factory = GetRiveFactory(context);
        if (!rive_factory)
        {
            dmLogError("No Rive factory available!");
            return rive::rcp<rive::Font>();
        }

        rive::Span<const uint8_t> data((const uint8_t*)resource, resource_size);
        return rive_factory->decodeFont(data);
    }

    rive::rcp<rive::Font> LoadFontFromFactory(dmResource::HFactory factory, HRenderContext context, const char* path)
    {
        if (!factory)
        {
            dmLogError("No factory provided!");
            return rive::rcp<rive::Font>();
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
            return rive::rcp<rive::Font>();
        }

        return LoadFontFromMemory(context, resource, resource_size);
    }

} // namespace dmRive