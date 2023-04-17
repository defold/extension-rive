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

#ifndef DM_RIVE_TESS_RENDERER_H
#define DM_RIVE_TESS_RENDERER_H

#include <rive/renderer.hpp>
#include <rive/tess/tess_render_path.hpp>
#include <rive/tess/tess_renderer.hpp>
#include <rive/tess/sub_path.hpp>

#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/graphics/graphics.h>

namespace dmRive {
    struct Atlas;
    struct Region;

    const char* BlendModeToStr(rive::BlendMode blendMode);

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

    class DefoldRenderPath;

    class DefoldRenderPaint : public rive::RenderPaint {
    private:
        FsUniforms                    m_uniforms = {0};
        rive::rcp<rive::RenderShader> m_shader;
        rive::RenderPaintStyle        m_style;

        std::unique_ptr<rive::ContourStroke> m_stroke;
        rive::StrokeJoin                     m_strokeJoin;
        rive::StrokeCap                      m_strokeCap;

        bool  m_strokeDirty     = false;
        float m_strokeThickness = 0.0f;

        dmArray<rive::Vec2D> m_strokeVertices;
        dmArray<uint16_t>    m_strokeIndices;
        dmArray<uint32_t>    m_strokeOffsets;

        rive::BlendMode m_blendMode = rive::BlendMode::srcOver;

    public:
        ~DefoldRenderPaint() override;
        void color(rive::ColorInt value) override;
        void style(rive::RenderPaintStyle value) override;
        rive::RenderPaintStyle style() const;
        void thickness(float value) override;
        void join(rive::StrokeJoin value) override;
        void cap(rive::StrokeCap value) override;
        void invalidateStroke() override;
        void blendMode(rive::BlendMode value) override;
        rive::BlendMode blendMode() const;
        void shader(rive::rcp<rive::RenderShader> shader) override;

        void draw(dmArray<DrawDescriptor>& drawDescriptors, VsUniforms& vertexUniforms, DefoldRenderPath* path, rive::BlendMode blendMode, uint8_t clipIndex);
    };

    class DefoldRenderPath : public rive::TessRenderPath {
    public:
        dmArray<rive::Vec2D> m_vertices;
        dmArray<uint16_t> m_indices;

        DefoldRenderPath();
        DefoldRenderPath(rive::RawPath& rawPath, rive::FillRule fillRule);
        ~DefoldRenderPath();

    protected:
        void addTriangles(rive::Span<const rive::Vec2D> vts, rive::Span<const uint16_t> ids) override;
        void setTriangulatedBounds(const rive::AABB& value) override;

    public:
        void reset() override;
        void drawStroke(rive::ContourStroke* stroke);
        DrawDescriptor drawFill();
    };

class DefoldTessRenderer : public rive::TessRenderer {
private:
    static const std::size_t maxClippingPaths = 16;
    int m_clipCount = 0;

    dmArray<rive::SubPath> m_ClipPaths;

    dmArray<DrawDescriptor> m_DrawDescriptors;

    dmArray<rive::Vec2D> m_ScratchBufferVec2D;
    dmArray<uint16_t> m_ScratchBufferIndices;

    Atlas* m_Atlas; // The current atlas lookup

    void          applyClipping();
    rive::Vec2D*  getRegionUvs(dmRive::Region* region, float* texcoords_in, int texcoords_in_count);
    void          putImage(DrawDescriptor& draw_desc, dmRive::Region* region, const rive::Mat2D& uv_transform);

public:
    DefoldTessRenderer();
    ~DefoldTessRenderer();
    void SetAtlas(Atlas* atlas);

    void orthographicProjection(float left,
                                float right,
                                float bottom,
                                float top,
                                float near,
                                float far) override;
    void drawPath(rive::RenderPath* path, rive::RenderPaint* paint) override;
    void drawImage(const rive::RenderImage*, rive::BlendMode, float opacity) override;
    void drawImageMesh(const rive::RenderImage*,
                       rive::rcp<rive::RenderBuffer> vertices_f32,
                       rive::rcp<rive::RenderBuffer> uvCoords_f32,
                       rive::rcp<rive::RenderBuffer> indices_u16,
                       rive::BlendMode,
                       float opacity) override;
    void restore() override;
    void reset();

    void getDrawDescriptors(DrawDescriptor** descriptors, uint32_t* count)
    {
        *descriptors = m_DrawDescriptors.Begin();
        *count = m_DrawDescriptors.Size();
    }
};

} // namespace dmRive

#endif
