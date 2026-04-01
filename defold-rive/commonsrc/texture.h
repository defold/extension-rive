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

#ifndef DM_RIVE_COMMONSRC_TEXTURE_H
#define DM_RIVE_COMMONSRC_TEXTURE_H

#include <stdint.h>

#include <dmsdk/graphics/graphics.h>

namespace dmRive
{
    struct TexturePixels
    {
        uint8_t*                 m_Data;
        uint32_t                 m_DataSize;
        uint16_t                 m_Width;
        uint16_t                 m_Height;
        dmGraphics::TextureFormat m_Format;
    };

    bool ReadPixelsBackBuffer(TexturePixels* out);
    bool ReadPixels(dmGraphics::HTexture texture, TexturePixels* out);
    void FreePixels(TexturePixels* pixels);
}

#endif // DM_RIVE_COMMONSRC_TEXTURE_H
