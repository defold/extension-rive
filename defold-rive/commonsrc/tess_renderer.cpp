#include <common/atlas.h>
#include <common/types.h>
#include <common/tess_renderer.h>
#include <common/factory.h>

// Based off of sokol_tess_renderer.hpp
#include <rive/renderer.hpp>
#include <rive/shapes/paint/color.hpp>
#include <rive/tess/tess_renderer.hpp>
#include <rive/tess/tess_render_path.hpp>
#include <rive/tess/contour_stroke.hpp>
#include <unordered_set>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/log.h>

template<typename T>
static void EnsureSize(dmArray<T>& a, uint32_t size)
{
    if (a.Capacity() < size)
    {
        a.SetCapacity(size);
    }
    a.SetSize(size);
}

static void fillColorBuffer(float* buffer, rive::ColorInt value) {
    buffer[0] = (float)rive::colorRed(value) / 0xFF;
    buffer[1] = (float)rive::colorGreen(value) / 0xFF;
    buffer[2] = (float)rive::colorBlue(value) / 0xFF;
    buffer[3] = rive::colorOpacity(value);
}

namespace dmRive
{
    const char* BlendModeToStr(rive::BlendMode blendMode)
    {
        switch(blendMode)
        {
            case rive::BlendMode::srcOver: return "BlendMode::srcOver";
            case rive::BlendMode::screen: return "BlendMode::screen";
            case rive::BlendMode::overlay: return "BlendMode::overlay";
            case rive::BlendMode::darken: return "BlendMode::darken";
            case rive::BlendMode::lighten: return "BlendMode::lighten";
            case rive::BlendMode::colorDodge: return "BlendMode::colorDodge";
            case rive::BlendMode::colorBurn: return "BlendMode::colorBurn";
            case rive::BlendMode::hardLight: return "BlendMode::hardLight";
            case rive::BlendMode::softLight: return "BlendMode::softLight";
            case rive::BlendMode::difference: return "BlendMode::difference";
            case rive::BlendMode::exclusion: return "BlendMode::exclusion";
            case rive::BlendMode::multiply: return "BlendMode::multiply";
            case rive::BlendMode::hue: return "BlendMode::hue";
            case rive::BlendMode::saturation: return "BlendMode::saturation";
            case rive::BlendMode::color: return "BlendMode::color";
            case rive::BlendMode::luminosity: return "BlendMode::luminosity";
        }
        return "";
    }

    class Gradient : public rive::RenderShader {
    private:
        rive::Vec2D m_start;
        rive::Vec2D m_end;
        int m_type;
        std::vector<float> m_colors;
        std::vector<float> m_stops;
        bool m_isVisible = false;

    private:
        // General gradient
        Gradient(int type, const rive::ColorInt colors[], const float stops[], size_t count) :
            m_type(type) {
            m_stops.resize(count);
            m_colors.resize(count * 4);
            for (int i = 0, colorIndex = 0; i < count; i++, colorIndex += 4) {
                fillColorBuffer(&m_colors[colorIndex], colors[i]);
                m_stops[i] = stops[i];
                if (m_colors[colorIndex + 3] > 0.0f) {
                    m_isVisible = true;
                }
            }
        }

        Gradient() {}

    public:
        virtual ~Gradient() {}

        // Linear gradient
        Gradient(float sx,
                 float sy,
                 float ex,
                 float ey,
                 const rive::ColorInt colors[],
                 const float stops[],
                 size_t count) :
            Gradient(1, colors, stops, count) {
            m_start = rive::Vec2D(sx, sy);
            m_end = rive::Vec2D(ex, ey);
        }

        Gradient(float cx,
                 float cy,
                 float radius,
                 const rive::ColorInt colors[], // [count]
                 const float stops[],     // [count]
                 size_t count) :
            Gradient(2, colors, stops, count) {
            m_start = rive::Vec2D(cx, cy);
            m_end = rive::Vec2D(cx + radius, cy);
        }

        void bind(VsUniforms& vertexUniforms, FsUniforms& fragmentUniforms)
        {
            auto stopCount = m_stops.size();
            vertexUniforms.fillType      = fragmentUniforms.fillType = m_type;
            vertexUniforms.gradientStart = m_start;
            vertexUniforms.gradientEnd   = m_end;
            fragmentUniforms.stopCount   = stopCount;
            for (int i = 0; i < stopCount; i++) {
                auto colorBufferIndex = i * 4;
                for (int j = 0; j < 4; j++) {
                    fragmentUniforms.colors[i][j] = m_colors[colorBufferIndex + j];
                }
                fragmentUniforms.stops[i / 4][i % 4] = m_stops[i];
            }
        }
    };

DefoldRenderImage::DefoldRenderImage(dmhash_t name_hash)
: m_NameHash(name_hash)
{
}

DefoldRenderPaint::~DefoldRenderPaint()
{
}

void DefoldRenderPaint::color(rive::ColorInt value)
{
    fillColorBuffer(m_uniforms.colors[0], value);
    m_uniforms.fillType = 0;
}

void DefoldRenderPaint::style(rive::RenderPaintStyle value)
{
    m_style = value;

    switch (m_style) {
        case rive::RenderPaintStyle::fill:
            m_stroke = nullptr;
            m_strokeDirty = false;
            break;
        case rive::RenderPaintStyle::stroke:
            m_stroke = std::make_unique<rive::ContourStroke>();
            m_strokeDirty = true;
            break;
    }
}

rive::RenderPaintStyle DefoldRenderPaint::style() const
{
    return m_style;
}

void DefoldRenderPaint::thickness(float value)
{
    m_strokeThickness = value;
    m_strokeDirty = true;
}

void DefoldRenderPaint::join(rive::StrokeJoin value)
{
    m_strokeJoin = value;
    m_strokeDirty = true;
}

void DefoldRenderPaint::cap(rive::StrokeCap value)
{
    m_strokeCap = value;
    m_strokeDirty = true;
}

void DefoldRenderPaint::invalidateStroke()
{
    if (m_stroke) {
        m_strokeDirty = true;
    }
}

void DefoldRenderPaint::blendMode(rive::BlendMode value)
{
    m_blendMode = value;
}
rive::BlendMode DefoldRenderPaint::blendMode() const
{
    return m_blendMode;
}

void DefoldRenderPaint::shader(rive::rcp<rive::RenderShader> shader)
{
    m_shader = shader;
}

static void PushDrawDescriptor(dmArray<DrawDescriptor>& drawDescriptors, DrawDescriptor desc)
{
    if (drawDescriptors.Full())
    {
        drawDescriptors.OffsetCapacity(8);
    }
    drawDescriptors.Push(desc);
}

void DefoldRenderPaint::draw(dmArray<DrawDescriptor>& drawDescriptors, VsUniforms& vertexUniforms, DefoldRenderPath* path, rive::BlendMode blendMode, uint8_t clipIndex)
{
    if (m_shader)
    {
        static_cast<Gradient*>(m_shader.get())->bind(vertexUniforms, m_uniforms);
    }

    if (m_stroke != nullptr)
    {
        if (m_strokeDirty)
        {
            static rive::Mat2D identity;
            m_stroke->reset();
            path->extrudeStroke(m_stroke.get(),
                                m_strokeJoin,
                                m_strokeCap,
                                m_strokeThickness / 2.0f,
                                identity);
            m_strokeDirty = false;

            const std::vector<rive::Vec2D>& strip = m_stroke->triangleStrip();

            auto size = strip.size();
            if (size <= 2) {
                return;
            }

            // Replace stroke buffer
            EnsureSize(m_strokeVertices, strip.size());
            memcpy(m_strokeVertices.Begin(), &strip.front(), strip.size() * sizeof(rive::Vec2D));

            // Let's use a tris index buffer so we can keep the same sokol pipeline.
            std::vector<uint16_t> indices;
            std::vector<uint32_t> offsets;

            // Build them by stroke offsets (where each offset represents a sub-path, or a move to)
            m_stroke->resetRenderOffset();
            // m_strokeOffsets.clear();
            while (true) {
                std::size_t strokeStart, strokeEnd;
                if (!m_stroke->nextRenderOffset(strokeStart, strokeEnd))
                {
                    break;
                }

                std::size_t length = strokeEnd - strokeStart;
                if (length > 2)
                {
                    for (std::size_t i = 0, end = length - 2; i < end; i++)
                    {
                        if ((i % 2) == 1) {
                            indices.push_back(i + strokeStart);
                            indices.push_back(i + 1 + strokeStart);
                            indices.push_back(i + 2 + strokeStart);
                        } else {
                            indices.push_back(i + strokeStart);
                            indices.push_back(i + 2 + strokeStart);
                            indices.push_back(i + 1 + strokeStart);
                        }
                    }
                    offsets.push_back(indices.size());
                }
            }

            // TODO: use dmArray directly instead of vector when creating
            EnsureSize(m_strokeIndices, indices.size());
            memcpy(m_strokeIndices.Begin(), &indices.front(), indices.size() * sizeof(uint16_t));

            EnsureSize(m_strokeOffsets, offsets.size());
            memcpy(m_strokeOffsets.Begin(), &offsets.front(), offsets.size() * sizeof(uint32_t));
        }

        m_stroke->resetRenderOffset();

        DrawDescriptor desc = {};
        desc.m_VsUniforms    = vertexUniforms;
        desc.m_FsUniforms    = m_uniforms;
        desc.m_Indices       = m_strokeIndices.Begin();
        desc.m_IndicesCount  = m_strokeIndices.Size();
        desc.m_Vertices      = m_strokeVertices.Begin();
        desc.m_VerticesCount = m_strokeVertices.Size();
        desc.m_DrawMode      = DRAW_MODE_DEFAULT;
        desc.m_BlendMode     = blendMode;
        desc.m_ClipIndex     = clipIndex;

        PushDrawDescriptor(drawDescriptors, desc);
    }
    else
    {
        DrawDescriptor desc = path->drawFill();

        desc.m_VsUniforms = vertexUniforms;
        desc.m_FsUniforms = m_uniforms;
        desc.m_ClipIndex  = clipIndex;
        desc.m_DrawMode   = DRAW_MODE_DEFAULT;
        desc.m_BlendMode  = blendMode;

        PushDrawDescriptor(drawDescriptors, desc);
    }
}

DefoldRenderPath::DefoldRenderPath()
{
}

DefoldRenderPath::DefoldRenderPath(rive::RawPath& rawPath, rive::FillRule fillRule) : rive::TessRenderPath(rawPath, fillRule)
{
}

DefoldRenderPath::~DefoldRenderPath()
{
}

void DefoldRenderPath::addTriangles(rive::Span<const rive::Vec2D> vts, rive::Span<const uint16_t> ids)
{
    uint32_t voffset = m_vertices.Size();
    uint32_t ioffset = m_indices.Size();
    EnsureSize(m_vertices, m_vertices.Size() + vts.size());
    EnsureSize(m_indices, m_indices.Size() + ids.size());
    memcpy(m_vertices.Begin()+voffset, vts.begin(), vts.size() * sizeof(rive::Vec2D));
    memcpy(m_indices.Begin()+ioffset, ids.begin(), ids.size() * sizeof(uint16_t));
}

void DefoldRenderPath::setTriangulatedBounds(const rive::AABB& value)
{
    // m_boundsIndex = m_vertices.size();
    // m_vertices.emplace_back(Vec2D(value.minX, value.minY));
    // m_vertices.emplace_back(Vec2D(value.maxX, value.minY));
    // m_vertices.emplace_back(Vec2D(value.maxX, value.maxY));
    // m_vertices.emplace_back(Vec2D(value.minX, value.maxY));
}

void DefoldRenderPath::reset()
{
    rive::TessRenderPath::reset();
    m_vertices.SetSize(0);
    m_indices.SetSize(0);
}

void DefoldRenderPath::drawStroke(rive::ContourStroke* stroke) {
    if (isContainer()) {
        for (auto& subPath : m_subPaths) {
            reinterpret_cast<DefoldRenderPath*>(subPath.path())->drawStroke(stroke);
        }
        return;
    }
    std::size_t start, end;
    stroke->nextRenderOffset(start, end);

    // what does this do?
    //sg_draw(start < 2 ? 0 : (start - 2) * 3, end - start < 2 ? 0 : (end - start - 2) * 3, 1);
}

DrawDescriptor DefoldRenderPath::drawFill()
{
    triangulate();

    DrawDescriptor desc = {};
    desc.m_Indices       = m_indices.Begin();
    desc.m_IndicesCount  = m_indices.Size();
    desc.m_Vertices      = m_vertices.Begin();
    desc.m_VerticesCount = m_vertices.Size();
    return desc;
}

DefoldTessRenderer::DefoldTessRenderer() {
    m_Atlas = 0;
}

DefoldTessRenderer::~DefoldTessRenderer() {
}

void DefoldTessRenderer::SetAtlas(Atlas* atlas) {
    m_Atlas = atlas;
}

void DefoldTessRenderer::orthographicProjection(float left,
                                               float right,
                                               float bottom,
                                               float top,
                                               float near,
                                               float far) {
    m_Projection[0] = 2.0f / (right - left);
    m_Projection[1] = 0.0f;
    m_Projection[2] = 0.0f;
    m_Projection[3] = 0.0f;

    m_Projection[4] = 0.0f;
    m_Projection[5] = 2.0f / (top - bottom);
    m_Projection[6] = 0.0f;
    m_Projection[7] = 0.0f;

    m_Projection[8] = 0.0f;
    m_Projection[9] = 0.0f;
    m_Projection[10] = 2.0f / (near - far);
    m_Projection[11] = 0.0f;

    m_Projection[12] = (right + left) / (left - right);
    m_Projection[13] = (top + bottom) / (bottom - top);
    m_Projection[14] = (far + near) / (near - far);
    m_Projection[15] = 1.0f;
}

static void print_mat2d(const rive::Mat2D& m)
{
    dmLogInfo(
        "[%f,%f\n %f,%f\n %f,%f]",
        m[0], m[1], m[2], m[3], m[4], m[5]);
}

void DefoldTessRenderer::putImage(DrawDescriptor& draw_desc, dmRive::Region* region, const rive::Mat2D& uv_transform)
{
    const int num_vertices  = 4;
    const int num_texcoords = 4;
    const int num_indices   = 6;
    const bool rotate       = region->degrees == 90;
    const float halfWidth   = region->dimensions[0] / 2.0f;
    const float halfHeight  = rotate ? 1.0 - region->dimensions[1] / 2.0f : region->dimensions[1] / 2.0f;

    uint32_t vx_index = m_ScratchBufferVec2D.Size();

    EnsureSize(m_ScratchBufferVec2D, vx_index + num_vertices + num_texcoords);

    rive::Vec2D* vx_write_ptr = &m_ScratchBufferVec2D[vx_index];
    vx_write_ptr[0]           = rive::Vec2D(-halfWidth, -halfHeight);
    vx_write_ptr[1]           = rive::Vec2D( halfWidth, -halfHeight);
    vx_write_ptr[2]           = rive::Vec2D( halfWidth,  halfHeight);
    vx_write_ptr[3]           = rive::Vec2D(-halfWidth,  halfHeight);

    rive::Vec2D* tc_write_ptr = vx_write_ptr + num_vertices;
    rive::Vec2D uv0(region->uv1[0], region->uv1[1]);
    rive::Vec2D uv1(region->uv2[0], region->uv1[1]);
    rive::Vec2D uv2(region->uv2[0], region->uv2[1]);
    rive::Vec2D uv3(region->uv1[0], region->uv2[1]);

    if (rotate)
    {
        tc_write_ptr[0] = uv_transform * uv2;
        tc_write_ptr[1] = uv_transform * uv1;
        tc_write_ptr[2] = uv_transform * uv0;
        tc_write_ptr[3] = uv_transform * uv3;
    }
    else
    {
        tc_write_ptr[0] = uv_transform * uv0;
        tc_write_ptr[1] = uv_transform * uv1;
        tc_write_ptr[2] = uv_transform * uv2;
        tc_write_ptr[3] = uv_transform * uv3;
    }

    uint32_t indices_index = m_ScratchBufferIndices.Size();
    EnsureSize(m_ScratchBufferIndices, m_ScratchBufferIndices.Size() + num_indices);
    uint16_t* indices_write_ptr = &m_ScratchBufferIndices[indices_index];

    indices_write_ptr[0] = 0;
    indices_write_ptr[1] = 1;
    indices_write_ptr[2] = 2;
    indices_write_ptr[3] = 0;
    indices_write_ptr[4] = 2;
    indices_write_ptr[5] = 3;

    draw_desc.m_Indices        = indices_write_ptr;
    draw_desc.m_IndicesCount   = num_indices;
    draw_desc.m_Vertices       = vx_write_ptr;
    draw_desc.m_VerticesCount  = num_vertices;
    draw_desc.m_TexCoords      = tc_write_ptr;
    draw_desc.m_TexCoordsCount = num_texcoords;
}

void DefoldTessRenderer::drawImage(const rive::RenderImage* _image, rive::BlendMode blendMode, float opacity)
{
    DefoldRenderImage* image = (DefoldRenderImage*)_image;

    dmRive::Region* region = dmRive::FindAtlasRegion(m_Atlas, image->m_NameHash);

    if (!region)
    {
        dmLogError("Couldn't find region '%s' in atlas", dmHashReverseSafe64(image->m_NameHash));
        return;
    }

    applyClipping();

    VsUniforms vs_params = {};
    vs_params.world = transform();

    FsUniforms fs_uniforms   = {0};
    fs_uniforms.fillType     = (int) FillType::FILL_TYPE_TEXTURED;
    fs_uniforms.colors[0][3] = opacity;

    DrawDescriptor desc = {};
    desc.m_VsUniforms     = vs_params;
    desc.m_FsUniforms     = fs_uniforms;
    desc.m_DrawMode       = DRAW_MODE_DEFAULT;
    desc.m_BlendMode      = blendMode;
    desc.m_ClipIndex      = m_clipCount;

    putImage(desc, region, image->uvTransform());

    PushDrawDescriptor(m_DrawDescriptors, desc);
}

rive::Vec2D* DefoldTessRenderer::getRegionUvs(dmRive::Region* region, float* texcoords_in, int texcoords_in_count)
{
    uint32_t tc_index = m_ScratchBufferVec2D.Size();
    EnsureSize(m_ScratchBufferVec2D, tc_index + texcoords_in_count);

    rive::Vec2D* texcoord_ptr = &m_ScratchBufferVec2D[tc_index];

    dmRive::ConvertRegionToAtlasUV(region, texcoords_in_count/2, texcoords_in, texcoord_ptr);
    return texcoord_ptr;
}

void DefoldTessRenderer::drawImageMesh(const rive::RenderImage* _image,
                                      rive::rcp<rive::RenderBuffer> vertices_f32,
                                      rive::rcp<rive::RenderBuffer> uvCoords_f32,
                                      rive::rcp<rive::RenderBuffer> indices_u16,
                                      rive::BlendMode blendMode,
                                      float opacity)
{
    DefoldRenderImage* image = (DefoldRenderImage*)_image;

    dmRive::Region* region = dmRive::FindAtlasRegion(m_Atlas, image->m_NameHash);
    if (!region)
    {
        dmLogError("Couldn't find region '%s' in atlas", dmHashReverseSafe64(image->m_NameHash));
        return;
    }

    applyClipping();

    DefoldBuffer<float>*   vertexbuffer = (DefoldBuffer<float>*)vertices_f32.get();
    DefoldBuffer<uint16_t>* indexbuffer = (DefoldBuffer<uint16_t>*)indices_u16.get();

    DefoldBuffer<float>* uvbuffer = (DefoldBuffer<float>*)uvCoords_f32.get();
    int count                     = uvbuffer->count();
    float* uvs                    = uvbuffer->m_Data;

    VsUniforms vs_params = {};
    vs_params.world = transform();

    FsUniforms fs_uniforms   = {0};
    fs_uniforms.fillType     = (int) FillType::FILL_TYPE_TEXTURED;
    fs_uniforms.colors[0][3] = opacity;

    DrawDescriptor desc = {};
    desc.m_VsUniforms     = vs_params;
    desc.m_FsUniforms     = fs_uniforms;
    desc.m_Indices        = indexbuffer->m_Data;
    desc.m_IndicesCount   = indexbuffer->count();
    desc.m_Vertices       = (rive::Vec2D*) vertexbuffer->m_Data;
    desc.m_VerticesCount  = vertexbuffer->count();
    desc.m_TexCoords      = getRegionUvs(region, uvs, count);
    desc.m_TexCoordsCount = count / 2;
    desc.m_DrawMode       = DRAW_MODE_DEFAULT;
    desc.m_BlendMode      = blendMode;
    desc.m_ClipIndex      = m_clipCount;

    PushDrawDescriptor(m_DrawDescriptors, desc);
}

void DefoldTessRenderer::restore() {
    TessRenderer::restore();

    if (m_Stack.size() == 1)
    {
        // When we've fully restored, immediately update clip to not wait for next draw.
        applyClipping();
    }
}

void DefoldTessRenderer::applyClipping() {
    if (!m_IsClippingDirty) {
        return;
    }
    m_IsClippingDirty = false;

    rive::RenderState& state = m_Stack.back();

    auto currentClipLength = m_ClipPaths.Size();
    if (currentClipLength == state.clipPaths.size()) {
        // Same length so now check if they're all the same.
        bool allSame = true;
        for (std::size_t i = 0; i < currentClipLength; i++) {
            if (state.clipPaths[i].path() != m_ClipPaths[i].path()) {
                allSame = false;
                break;
            }
        }
        if (allSame) {
            return;
        }
    }

    VsUniforms vs_uniforms = { .fillType = 0 };
    FsUniforms fs_uniforms = {0};

    // Decr any paths from the last clip that are gone.
    std::unordered_set<rive::RenderPath*> alreadyApplied;

    for (int i = 0; i < m_ClipPaths.Size(); ++i)
    {
        auto appliedPath = m_ClipPaths[i];
        bool decr = true;
        for (auto nextClipPath : state.clipPaths) {
            if (nextClipPath.path() == appliedPath.path()) {
                decr = false;
                alreadyApplied.insert(appliedPath.path());
                break;
            }
        }
        if (decr) {
            // Draw appliedPath.path() with decr pipeline
            vs_uniforms.world = appliedPath.transform();

            auto path = static_cast<DefoldRenderPath*>(appliedPath.path());
            DrawDescriptor desc = path->drawFill();

            desc.m_VsUniforms = vs_uniforms;
            desc.m_FsUniforms = fs_uniforms;
            desc.m_DrawMode   = DRAW_MODE_CLIP_DECR;

            PushDrawDescriptor(m_DrawDescriptors, desc);
        }
    }

    // Incr any paths that are added.
    for (int i = 0; i < state.clipPaths.size(); ++i)
    {
        auto nextClipPath = state.clipPaths[i];
        if (alreadyApplied.count(nextClipPath.path())) {
            // Already applied.
            continue;
        }
        // Draw nextClipPath.path() with incr pipeline
        vs_uniforms.world = nextClipPath.transform();
        auto path = static_cast<DefoldRenderPath*>(nextClipPath.path());
        DrawDescriptor desc = path->drawFill();

        desc.m_VsUniforms = vs_uniforms;
        desc.m_FsUniforms = fs_uniforms;
        desc.m_DrawMode   = DRAW_MODE_CLIP_INCR;

        PushDrawDescriptor(m_DrawDescriptors, desc);
    }

    // Pick which pipeline to use for draw path operations.
    // TODO: something similar for draw mesh.
    m_clipCount = state.clipPaths.size();

    EnsureSize(m_ClipPaths, state.clipPaths.size());
    memcpy(m_ClipPaths.Begin(), &state.clipPaths.front(), state.clipPaths.size() * sizeof(rive::SubPath));
}

void DefoldTessRenderer::reset()
{
    m_DrawDescriptors.SetSize(0);
    m_ScratchBufferVec2D.SetSize(0);
    m_ScratchBufferIndices.SetSize(0);
}

void DefoldTessRenderer::drawPath(rive::RenderPath* path, rive::RenderPaint* _paint) {
    auto paint = static_cast<DefoldRenderPaint*>(_paint);

    applyClipping();

    VsUniforms vs_params = {};
    vs_params.world = transform();

    static_cast<DefoldRenderPaint*>(paint)->draw(m_DrawDescriptors, vs_params, static_cast<DefoldRenderPath*>(path), paint->blendMode(), m_clipCount);
}

// The factory implementations are here since they belong to the actual renderer.

rive::rcp<rive::RenderShader> DefoldFactory::makeLinearGradient(float sx,
                                                   float sy,
                                                   float ex,
                                                   float ey,
                                                   const rive::ColorInt colors[],
                                                   const float stops[],
                                                   size_t count) {
    return rive::rcp<rive::RenderShader>(new Gradient(sx, sy, ex, ey, colors, stops, count));
}

rive::rcp<rive::RenderShader> DefoldFactory::makeRadialGradient(float cx,
                                                   float cy,
                                                   float radius,
                                                   const rive::ColorInt colors[], // [count]
                                                   const float stops[],     // [count]
                                                   size_t count) {
    return rive::rcp<rive::RenderShader>(new Gradient(cx, cy, radius, colors, stops, count));
}

std::unique_ptr<rive::RenderPaint> DefoldFactory::makeRenderPaint() {
    return std::make_unique<DefoldRenderPaint>();
}

// Returns a full-formed RenderPath -- can be treated as immutable
std::unique_ptr<rive::RenderPath> DefoldFactory::makeRenderPath(rive::RawPath& rawPath, rive::FillRule rule) {
    return std::make_unique<DefoldRenderPath>(rawPath, rule);
}

std::unique_ptr<rive::RenderPath> DefoldFactory::makeEmptyRenderPath() {
    return std::make_unique<DefoldRenderPath>();
}

std::unique_ptr<rive::RenderImage> DefoldFactory::decodeImage(rive::Span<const uint8_t> data)
{
    return nullptr;
}

} // namespace dmRive
