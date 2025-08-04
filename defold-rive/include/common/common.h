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

#ifndef DM_RIVE_COMMON_H
#define DM_RIVE_COMMON_H

#include <rive/renderer.hpp>
#include <rive/tess/tess_render_path.hpp>
#include <rive/tess/tess_renderer.hpp>
#include <rive/tess/sub_path.hpp>

#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/graphics/graphics.h>

namespace dmRive {
    struct VsUniforms
    {
        rive::Mat4 world;
        rive::Vec2D gradientStart;
        rive::Vec2D gradientEnd;
        int fillType;
    };

    struct FsUniforms
    {
        int   fillType;
        float colors[16][4];
        float stops[4][4];
        int   stopCount;
    };

    enum DrawMode
    {
        DRAW_MODE_DEFAULT   = 0,
        DRAW_MODE_CLIP_DECR = 1,
        DRAW_MODE_CLIP_INCR = 2,
    };

    // Must match shader fill type
    // Note: The 'texrtured' fill type is a Defold fill type so we can use the same shader for all content
    enum FillType
    {
        FILL_TYPE_SOLID    = 0,
        FILL_TYPE_LINEAR   = 1,
        FILL_TYPE_RADIAL   = 2,
        FILL_TYPE_TEXTURED = 3,
    };

    struct DrawDescriptor
    {
        VsUniforms      m_VsUniforms;
        FsUniforms      m_FsUniforms;
        rive::BlendMode m_BlendMode;
        rive::Vec2D*    m_Vertices;
        rive::Vec2D*    m_TexCoords;
        uint16_t*       m_Indices;
        DrawMode        m_DrawMode;
        uint32_t        m_VerticesCount;
        uint32_t        m_IndicesCount;
        uint32_t        m_TexCoordsCount;
        uint8_t         m_ClipIndex;
    };

} // namespace dmRive

#endif
