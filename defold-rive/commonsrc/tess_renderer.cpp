// #include "rive/tess/sokol/sokol_tess_renderer.hpp"
// #include "rive/tess/sokol/sokol_factory.hpp"

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
//#include <generated/shader.h>
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

void DefoldRenderPaint::draw(dmArray<DrawDescriptor>& drawDescriptors, VsUniforms& vertexUniforms, DefoldRenderPath* path, rive::BlendMode blendMode, uint8_t clipIndex)
{
    if (m_shader)
    {
        static_cast<Gradient*>(m_shader.get())->bind(vertexUniforms, m_uniforms);
    }

    // printf("paint.draw\n");

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
                if (!m_stroke->nextRenderOffset(strokeStart, strokeEnd)) {
                    break;
                }

                // printf("SS: %d, SE: %d\n", (int) strokeStart, (int) strokeEnd);

                std::size_t length = strokeEnd - strokeStart;
                if (length > 2) {
                    for (std::size_t i = 0, end = length - 2; i < end; i++) {
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

            /*
            m_strokeIndexBuffer = sg_make_buffer((sg_buffer_desc){
                .type = SG_BUFFERTYPE_INDEXBUFFER,
                .data =
                    {
                        indices.data(),
                        indices.size() * sizeof(uint16_t),
                    },
            });
            */
        }

        /*
        if (m_strokeVertexBuffer.id == 0) {
            return;
        }

        sg_bindings bind = {
            .vertex_buffers[0] = m_strokeVertexBuffer,
            .index_buffer = m_strokeIndexBuffer,
        };

        sg_apply_bindings(&bind);
        */

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

        if (drawDescriptors.Full())
        {
            drawDescriptors.OffsetCapacity(8);
        }
        drawDescriptors.Push(desc);
    }
    else
    {
        DrawDescriptor desc = path->drawFill();

        desc.m_VsUniforms = vertexUniforms;
        desc.m_FsUniforms = m_uniforms;
        desc.m_ClipIndex  = clipIndex;
        desc.m_DrawMode   = DRAW_MODE_DEFAULT;
        desc.m_BlendMode  = blendMode;

        if (drawDescriptors.Full())
        {
            drawDescriptors.OffsetCapacity(8);
        }
        drawDescriptors.Push(desc);
    }
}

// void draw(vs_path_params_t& vertexUniforms, DefoldRenderPath* path) {
//     if (m_shader) {
//         static_cast<Gradient*>(m_shader.get())->bind(vertexUniforms, m_uniforms);
//     }

//     sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_path_params, SG_RANGE_REF(vertexUniforms));
//     sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_fs_path_uniforms, SG_RANGE_REF(m_uniforms));
//     if (m_stroke != nullptr) {
//         if (m_strokeDirty) {
//             static Mat2D identity;
//             m_stroke->reset();
//             path->extrudeStroke(m_stroke.get(),
//                                 m_strokeJoin,
//                                 m_strokeCap,
//                                 m_strokeThickness / 2.0f,
//                                 identity);
//             m_strokeDirty = false;

//             const std::vector<Vec2D>& strip = m_stroke->triangleStrip();

//             sg_destroy_buffer(m_strokeVertexBuffer);
//             sg_destroy_buffer(m_strokeIndexBuffer);
//             auto size = strip.size();
//             if (size <= 2) {
//                 m_strokeVertexBuffer = {0};
//                 m_strokeIndexBuffer = {0};
//                 return;
//             }

//             m_strokeVertexBuffer = sg_make_buffer((sg_buffer_desc){
//                 .type = SG_BUFFERTYPE_VERTEXBUFFER,
//                 .data =
//                     {
//                         strip.data(),
//                         strip.size() * sizeof(Vec2D),
//                     },
//             });

//             // Let's use a tris index buffer so we can keep the same sokol pipeline.
//             std::vector<uint16_t> indices;

//             // Build them by stroke offsets (where each offset represents a sub-path, or a move
//             // to)
//             m_stroke->resetRenderOffset();
//             m_StrokeOffsets.clear();
//             while (true) {
//                 std::size_t strokeStart, strokeEnd;
//                 if (!m_stroke->nextRenderOffset(strokeStart, strokeEnd)) {
//                     break;
//                 }
//                 std::size_t length = strokeEnd - strokeStart;
//                 if (length > 2) {
//                     for (std::size_t i = 0, end = length - 2; i < end; i++) {
//                         if ((i % 2) == 1) {
//                             indices.push_back(i + strokeStart);
//                             indices.push_back(i + 1 + strokeStart);
//                             indices.push_back(i + 2 + strokeStart);
//                         } else {
//                             indices.push_back(i + strokeStart);
//                             indices.push_back(i + 2 + strokeStart);
//                             indices.push_back(i + 1 + strokeStart);
//                         }
//                     }
//                     m_StrokeOffsets.push_back(indices.size());
//                 }
//             }

//             m_strokeIndexBuffer = sg_make_buffer((sg_buffer_desc){
//                 .type = SG_BUFFERTYPE_INDEXBUFFER,
//                 .data =
//                     {
//                         indices.data(),
//                         indices.size() * sizeof(uint16_t),
//                     },
//             });
//         }
//         if (m_strokeVertexBuffer.id == 0) {
//             return;
//         }

//         sg_bindings bind = {
//             .vertex_buffers[0] = m_strokeVertexBuffer,
//             .index_buffer = m_strokeIndexBuffer,
//         };

//         sg_apply_bindings(&bind);

//         m_stroke->resetRenderOffset();
//         // path->drawStroke(m_stroke.get());
//         std::size_t start = 0;
//         for (auto end : m_StrokeOffsets) {
//             sg_draw(start, end - start, 1);
//             start = end;
//         }

//     } else {
//         path->drawFill();
//     }
// }

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

    // if (triangulate()) {
    //     sg_destroy_buffer(m_vertexBuffer);
    //     sg_destroy_buffer(m_indexBuffer);
    //     if (m_indices.size() == 0 || m_vertices.size() == 0) {
    //         m_vertexBuffer = {0};
    //         m_indexBuffer = {0};
    //         return;
    //     }

    //     m_vertexBuffer = sg_make_buffer((sg_buffer_desc){
    //         .type = SG_BUFFERTYPE_VERTEXBUFFER,
    //         .data =
    //             {
    //                 m_vertices.data(),
    //                 m_vertices.size() * sizeof(Vec2D),
    //             },
    //     });

    //     m_indexBuffer = sg_make_buffer((sg_buffer_desc){
    //         .type = SG_BUFFERTYPE_INDEXBUFFER,
    //         .data =
    //             {
    //                 m_indices.data(),
    //                 m_indices.size() * sizeof(uint16_t),
    //             },
    //     });
    // }

    // if (m_vertexBuffer.id == 0) {
    //     return;
    // }

    // sg_bindings bind = {
    //     .vertex_buffers[0] = m_vertexBuffer,
    //     .index_buffer = m_indexBuffer,
    // };

    // sg_apply_bindings(&bind);
    // sg_draw(0, m_indices.size(), 1);
}

// class SokolBuffer : public RenderBuffer {
// private:
//     sg_buffer m_Buffer;

// public:
//     SokolBuffer(size_t count, const sg_buffer_desc& desc) :
//         RenderBuffer(count), m_Buffer(sg_make_buffer(desc)) {}
//     ~SokolBuffer() { sg_destroy_buffer(m_Buffer); }

//     sg_buffer buffer() { return m_Buffer; }
// };

// rcp<RenderBuffer> SokolFactory::makeBufferU16(Span<const uint16_t> span) {
//     return rcp<RenderBuffer>(new SokolBuffer(span.size(),
//                                              (sg_buffer_desc){
//                                                  .type = SG_BUFFERTYPE_INDEXBUFFER,
//                                                  .data =
//                                                      {
//                                                          span.data(),
//                                                          span.size_bytes(),
//                                                      },
//                                              }));
// }

// rcp<RenderBuffer> SokolFactory::makeBufferU32(Span<const uint32_t> span) {
//     return rcp<RenderBuffer>(new SokolBuffer(span.size(),
//                                              (sg_buffer_desc){
//                                                  .type = SG_BUFFERTYPE_INDEXBUFFER,
//                                                  .data =
//                                                      {
//                                                          span.data(),
//                                                          span.size_bytes(),
//                                                      },
//                                              }));
// }
// rcp<RenderBuffer> SokolFactory::makeBufferF32(Span<const float> span) {
//     return rcp<RenderBuffer>(new SokolBuffer(span.size(),
//                                              (sg_buffer_desc){
//                                                  .type = SG_BUFFERTYPE_VERTEXBUFFER,
//                                                  .data =
//                                                      {
//                                                          span.data(),
//                                                          span.size_bytes(),
//                                                      },
//                                              }));
// }

// sg_pipeline vectorPipeline(sg_shader shader,
//                            sg_blend_state blend,
//                            sg_stencil_state stencil,
//                            sg_color_mask colorMask = SG_COLORMASK_RGBA) {
//     return sg_make_pipeline((sg_pipeline_desc){
//         .layout =
//             {
//                 .attrs =
//                     {
//                         [ATTR_vs_path_position] =
//                             {
//                                 .format = SG_VERTEXFORMAT_FLOAT2,
//                                 .buffer_index = 0,
//                             },
//                     },
//             },
//         .shader = shader,
//         .index_type = SG_INDEXTYPE_UINT16,
//         .cull_mode = SG_CULLMODE_NONE,
//         .depth =
//             {
//                 .compare = SG_COMPAREFUNC_ALWAYS,
//                 .write_enabled = false,
//             },
//         .colors =
//             {
//                 [0] =
//                     {
//                         .write_mask = colorMask,
//                         .blend = blend,
//                     },
//             },
//         .stencil = stencil,
//         .label = "path-pipeline",
//     });
// }

DefoldTessRenderer::DefoldTessRenderer() {
    m_Atlas = 0;

    // m_meshPipeline = sg_make_pipeline((sg_pipeline_desc){
    //     .layout =
    //         {
    //             .attrs =
    //                 {
    //                     [ATTR_vs_position] = {.format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 0},
    //                     [ATTR_vs_texcoord0] = {.format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1},
    //                 },
    //         },
    //     .shader = sg_make_shader(rive_tess_shader_desc(sg_query_backend())),
    //     .index_type = SG_INDEXTYPE_UINT16,
    //     .cull_mode = SG_CULLMODE_NONE,
    //     .depth =
    //         {
    //             .compare = SG_COMPAREFUNC_ALWAYS,
    //             .write_enabled = false,
    //         },
    //     .colors =
    //         {
    //             [0] =
    //                 {
    //                     .write_mask = SG_COLORMASK_RGBA,
    //                     .blend =
    //                         {
    //                             .enabled = true,
    //                             .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
    //                             .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
    //                         },
    //                 },
    //         },
    //     .label = "mesh-pipeline",
    // });

    // auto uberShader = sg_make_shader(rive_tess_path_shader_desc(sg_query_backend()));

    // assert(maxClippingPaths < 256);

    // // Src Over Pipelines
    // {
    //     m_pathPipeline[0] = vectorPipeline(uberShader,
    //                                        {
    //                                            .enabled = true,
    //                                            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
    //                                            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
    //                                        },
    //                                        {
    //                                            .enabled = false,
    //                                        });

    //     for (std::size_t i = 1; i <= maxClippingPaths; i++) {
    //         m_pathPipeline[i] =
    //             vectorPipeline(uberShader,
    //                            {
    //                                .enabled = true,
    //                                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
    //                                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
    //                            },
    //                            {
    //                                .enabled = true,
    //                                .ref = (uint8_t)i,
    //                                .read_mask = 0xFF,
    //                                .write_mask = 0x00,
    //                                .front =
    //                                    {
    //                                        .compare = SG_COMPAREFUNC_EQUAL,
    //                                    },
    //                                .back =
    //                                    {
    //                                        .compare = SG_COMPAREFUNC_EQUAL,
    //                                    },
    //                            });
    //     }
    // }

    // // Screen Pipelines
    // {
    //     m_pathScreenPipeline[0] =
    //         vectorPipeline(uberShader,
    //                        {
    //                            .enabled = true,
    //                            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
    //                            .dst_factor_rgb = SG_BLENDFACTOR_ONE,
    //                            .src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA,
    //                            .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
    //                        },
    //                        {
    //                            .enabled = false,
    //                        });

    //     for (std::size_t i = 1; i <= maxClippingPaths; i++) {
    //         m_pathScreenPipeline[i] =
    //             vectorPipeline(uberShader,
    //                            {
    //                                .enabled = true,
    //                                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
    //                                .dst_factor_rgb = SG_BLENDFACTOR_ONE,
    //                                .src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA,
    //                                .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
    //                            },
    //                            {
    //                                .enabled = true,
    //                                .ref = (uint8_t)i,
    //                                .read_mask = 0xFF,
    //                                .write_mask = 0x00,
    //                                .front =
    //                                    {
    //                                        .compare = SG_COMPAREFUNC_EQUAL,
    //                                    },
    //                                .back =
    //                                    {
    //                                        .compare = SG_COMPAREFUNC_EQUAL,
    //                                    },
    //                            });
    //     }
    // }

    // // Additive Pipelines
    // {
    //     m_pathAdditivePipeline[0] = vectorPipeline(uberShader,
    //                                                {
    //                                                    .enabled = true,
    //                                                    .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
    //                                                    .dst_factor_rgb = SG_BLENDFACTOR_ONE,
    //                                                },
    //                                                {
    //                                                    .enabled = false,
    //                                                });

    //     for (std::size_t i = 1; i <= maxClippingPaths; i++) {
    //         m_pathAdditivePipeline[i] =
    //             vectorPipeline(uberShader,
    //                            {
    //                                .enabled = true,
    //                                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
    //                                .dst_factor_rgb = SG_BLENDFACTOR_ONE,
    //                            },
    //                            {
    //                                .enabled = true,
    //                                .ref = (uint8_t)i,
    //                                .read_mask = 0xFF,
    //                                .write_mask = 0x00,
    //                                .front =
    //                                    {
    //                                        .compare = SG_COMPAREFUNC_EQUAL,
    //                                    },
    //                                .back =
    //                                    {
    //                                        .compare = SG_COMPAREFUNC_EQUAL,
    //                                    },
    //                            });
    //     }
    // }

    // // Multiply Pipelines
    // {
    //     m_pathMultiplyPipeline[0] =
    //         vectorPipeline(uberShader,
    //                        {
    //                            .enabled = true,
    //                            .src_factor_rgb = SG_BLENDFACTOR_DST_COLOR,
    //                            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
    //                        },
    //                        {
    //                            .enabled = false,
    //                        });

    //     for (std::size_t i = 1; i <= maxClippingPaths; i++) {
    //         m_pathMultiplyPipeline[i] =
    //             vectorPipeline(uberShader,
    //                            {
    //                                .enabled = true,
    //                                .src_factor_rgb = SG_BLENDFACTOR_DST_COLOR,
    //                                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
    //                            },
    //                            {
    //                                .enabled = true,
    //                                .ref = (uint8_t)i,
    //                                .read_mask = 0xFF,
    //                                .write_mask = 0x00,
    //                                .front =
    //                                    {
    //                                        .compare = SG_COMPAREFUNC_EQUAL,
    //                                    },
    //                                .back =
    //                                    {
    //                                        .compare = SG_COMPAREFUNC_EQUAL,
    //                                    },
    //                            });
    //     }
    // }

    // m_incClipPipeline = vectorPipeline(uberShader,
    //                                    {
    //                                        .enabled = false,
    //                                    },
    //                                    {
    //                                        .enabled = true,
    //                                        .read_mask = 0xFF,
    //                                        .write_mask = 0xFF,
    //                                        .front =
    //                                            {
    //                                                .compare = SG_COMPAREFUNC_ALWAYS,
    //                                                .depth_fail_op = SG_STENCILOP_KEEP,
    //                                                .fail_op = SG_STENCILOP_KEEP,
    //                                                .pass_op = SG_STENCILOP_INCR_CLAMP,
    //                                            },
    //                                        .back =
    //                                            {
    //                                                .compare = SG_COMPAREFUNC_ALWAYS,
    //                                                .depth_fail_op = SG_STENCILOP_KEEP,
    //                                                .fail_op = SG_STENCILOP_KEEP,
    //                                                .pass_op = SG_STENCILOP_INCR_CLAMP,
    //                                            },
    //                                    },
    //                                    SG_COLORMASK_NONE);

    // m_decClipPipeline = vectorPipeline(uberShader,
    //                                    {
    //                                        .enabled = false,
    //                                    },
    //                                    {
    //                                        .enabled = true,
    //                                        .read_mask = 0xFF,
    //                                        .write_mask = 0xFF,
    //                                        .front =
    //                                            {
    //                                                .compare = SG_COMPAREFUNC_ALWAYS,
    //                                                .depth_fail_op = SG_STENCILOP_KEEP,
    //                                                .fail_op = SG_STENCILOP_KEEP,
    //                                                .pass_op = SG_STENCILOP_DECR_CLAMP,
    //                                            },
    //                                        .back =
    //                                            {
    //                                                .compare = SG_COMPAREFUNC_ALWAYS,
    //                                                .depth_fail_op = SG_STENCILOP_KEEP,
    //                                                .fail_op = SG_STENCILOP_KEEP,
    //                                                .pass_op = SG_STENCILOP_DECR_CLAMP,
    //                                            },
    //                                    },
    //                                    SG_COLORMASK_NONE);

    // uint16_t indices[] = {0, 1, 2, 0, 2, 3};

    // m_boundsIndices = sg_make_buffer((sg_buffer_desc){
    //     .type = SG_BUFFERTYPE_INDEXBUFFER,
    //     .data = SG_RANGE(indices),
    // });

}

DefoldTessRenderer::~DefoldTessRenderer() {
    // sg_destroy_buffer(m_boundsIndices);
    // sg_destroy_pipeline(m_meshPipeline);
    // sg_destroy_pipeline(m_incClipPipeline);
    // sg_destroy_pipeline(m_decClipPipeline);
    // for (std::size_t i = 0; i <= maxClippingPaths; i++) {
    //     sg_destroy_pipeline(m_pathPipeline[i]);
    //     sg_destroy_pipeline(m_pathScreenPipeline[i]);
    // }
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

//#ifdef SOKOL_GLCORE33
    // Use OpenGL version
    m_Projection[8] = 0.0f;
    m_Projection[9] = 0.0f;
    m_Projection[10] = 2.0f / (near - far);
    m_Projection[11] = 0.0f;

    m_Projection[12] = (right + left) / (left - right);
    m_Projection[13] = (top + bottom) / (bottom - top);
    m_Projection[14] = (far + near) / (near - far);
    m_Projection[15] = 1.0f;
// #else
//     // NDC are slightly different in Metal:
//     // https://metashapes.com/blog/opengl-metal-projection-matrix-problem/
//     m_Projection[8] = 0.0f;
//     m_Projection[9] = 0.0f;
//     m_Projection[10] = 1.0f / (far - near);
//     m_Projection[11] = 0.0f;

//     m_Projection[12] = (right + left) / (left - right);
//     m_Projection[13] = (top + bottom) / (bottom - top);
//     m_Projection[14] = near / (near - far);
//     m_Projection[15] = 1.0f;
// #endif
    // for (int i = 0; i < 4; i++) {
    //     int b = i * 4;
    //     printf("%f\t%f\t%f\t%f\n",
    //            m_Projection[b],
    //            m_Projection[b + 1],
    //            m_Projection[b + 2],
    //            m_Projection[b + 3]);
    // }
}

void DefoldTessRenderer::drawImage(const rive::RenderImage* _image, rive::BlendMode, float opacity) {
    DefoldRenderImage* image = (DefoldRenderImage*)_image;
    //printf("drawImage '%s'\n", image?dmHashReverseSafe64(image->m_NameHash): "null");
    // vs_params_t vs_params;

    // const Mat2D& world = transform();
    // vs_params.mvp = m_Projection * world;

    // auto sokolImage = static_cast<const SokolRenderImage*>(image);
    // setPipeline(m_meshPipeline);
    // sg_bindings bind = {
    //     .vertex_buffers[0] = sokolImage->vertexBuffer(),
    //     .vertex_buffers[1] = sokolImage->uvBuffer(),
    //     .index_buffer = m_boundsIndices,
    //     .fs_images[SLOT_tex] = sokolImage->image(),
    // };

    // sg_apply_bindings(&bind);
    // sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, SG_RANGE_REF(vs_params));
    // sg_draw(0, 6, 1);
}

void DefoldTessRenderer::drawImageMesh(const rive::RenderImage* _image,
                                      rive::rcp<rive::RenderBuffer> vertices_f32,
                                      rive::rcp<rive::RenderBuffer> uvCoords_f32,
                                      rive::rcp<rive::RenderBuffer> indices_u16,
                                      rive::BlendMode blendMode,
                                      float opacity) {
    DefoldRenderImage* image = (DefoldRenderImage*)_image;
    //printf("drawImageMesh '%s'\n", dmHashReverseSafe64(image->m_NameHash));

    if (!m_Atlas)
        return;

    dmRive::Region* region = dmRive::FindAtlasRegion(m_Atlas, image->m_NameHash);
    if (!region) {
        dmLogError("Couldn't find region '%s' in atlas", dmHashReverseSafe64(image->m_NameHash));
        return;
    }

    DefoldBuffer<float>* uvbuffer = (DefoldBuffer<float>*)uvCoords_f32.get();
    int count = uvbuffer->count();
    float* uvs = uvbuffer->m_Data;

    //dmRive::ConvertRegionToAtlasUV(region, count/2, uvs, outuvs); // TODO: Where to write to?


    // vs_params_t vs_params;

    // const Mat2D& world = transform();
    // vs_params.mvp = m_Projection * world;

    // setPipeline(m_meshPipeline);
    // sg_bindings bind = {
    //     .vertex_buffers[0] = static_cast<SokolBuffer*>(vertices_f32.get())->buffer(),
    //     .vertex_buffers[1] = static_cast<SokolBuffer*>(uvCoords_f32.get())->buffer(),
    //     .index_buffer = static_cast<SokolBuffer*>(indices_u16.get())->buffer(),
    //     .fs_images[SLOT_tex] = static_cast<const SokolRenderImage*>(renderImage)->image(),
    // };

    // sg_apply_bindings(&bind);
    // sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, SG_RANGE_REF(vs_params));
    // sg_draw(0, indices_u16->count(), 1);
}


void DefoldTessRenderer::restore() {
    TessRenderer::restore();
    if (m_Stack.size() == 1) {
        // When we've fully restored, immediately update clip to not wait for next draw.
        applyClipping();
        //m_currentPipeline = {0};
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

            // setPipeline(m_decClipPipeline);
            vs_uniforms.world = appliedPath.transform();

            //sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_path_params, SG_RANGE_REF(vs_uniforms));
            //sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_fs_path_uniforms, SG_RANGE_REF(uniforms));
            auto path = static_cast<DefoldRenderPath*>(appliedPath.path());
            DrawDescriptor desc = path->drawFill();

            desc.m_VsUniforms = vs_uniforms;
            desc.m_FsUniforms = fs_uniforms;
            desc.m_DrawMode   = DRAW_MODE_CLIP_DECR;

            if (m_DrawDescriptors.Full())
            {
                m_DrawDescriptors.OffsetCapacity(8);
            }
            m_DrawDescriptors.Push(desc);
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
        // setPipeline(m_incClipPipeline);
        vs_uniforms.world = nextClipPath.transform();
        // sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_path_params, SG_RANGE_REF(vs_uniforms));
        // sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_fs_path_uniforms, SG_RANGE_REF(uniforms));
        auto path = static_cast<DefoldRenderPath*>(nextClipPath.path());
        DrawDescriptor desc = path->drawFill();

        desc.m_VsUniforms = vs_uniforms;
        desc.m_FsUniforms = fs_uniforms;
        desc.m_DrawMode   = DRAW_MODE_CLIP_INCR;

        if (m_DrawDescriptors.Full())
        {
            m_DrawDescriptors.OffsetCapacity(8);
        }
        m_DrawDescriptors.Push(desc);
    }

    // Pick which pipeline to use for draw path operations.
    // TODO: something similar for draw mesh.
    m_clipCount = state.clipPaths.size();

    EnsureSize(m_ClipPaths, state.clipPaths.size());
    memcpy(m_ClipPaths.Begin(), &state.clipPaths.front(), state.clipPaths.size() * sizeof(rive::SubPath));

    // m_ClipPaths = state.clipPaths;
}

void DefoldTessRenderer::reset()
{
    m_DrawDescriptors.SetSize(0);

    //m_currentPipeline = {0};
}

// void DefoldTessRenderer::setPipeline(sg_pipeline pipeline)
// {
//     if (m_currentPipeline.id == pipeline.id) {
//         return;
//     }
//     m_currentPipeline = pipeline;
//     sg_apply_pipeline(pipeline);
// }

void DefoldTessRenderer::drawPath(rive::RenderPath* path, rive::RenderPaint* _paint) {
    auto paint = static_cast<DefoldRenderPaint*>(_paint);

    applyClipping();
    // vs_path_params_t vs_params = {.fillType = 0};
    // const Mat2D& world = transform();

    // vs_params.mvp = m_Projection * world;
    // switch (sokolPaint->blendMode()) {
    //     case BlendMode::srcOver: setPipeline(m_pathPipeline[m_clipCount]); break;
    //     case BlendMode::screen: setPipeline(m_pathScreenPipeline[m_clipCount]); break;
    //     case BlendMode::colorDodge: setPipeline(m_pathAdditivePipeline[m_clipCount]); break;
    //     case BlendMode::multiply: setPipeline(m_pathMultiplyPipeline[m_clipCount]); break;
    //     default: setPipeline(m_pathScreenPipeline[m_clipCount]); break;
    // }

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

// SokolRenderImageResource::SokolRenderImageResource(const uint8_t* bytes,
//                                                    uint32_t width,
//                                                    uint32_t height) :
//     m_gpuResource(sg_make_image((sg_image_desc){
//         .width = (int)width,
//         .height = (int)height,
//         .data.subimage[0][0] = {bytes, width * height * 4},
//         .pixel_format = SG_PIXELFORMAT_RGBA8,
//     })) {}
// SokolRenderImageResource::~SokolRenderImageResource() { sg_destroy_image(m_gpuResource); }

// SokolRenderImage::SokolRenderImage(rcp<SokolRenderImageResource> image,
//                                    uint32_t width,
//                                    uint32_t height,
//                                    const Mat2D& uvTransform) :

//     RenderImage(uvTransform),
//     m_gpuImage(image)

// {
//     float halfWidth = width / 2.0f;
//     float halfHeight = height / 2.0f;
//     Vec2D points[] = {
//         Vec2D(-halfWidth, -halfHeight),
//         Vec2D(halfWidth, -halfHeight),
//         Vec2D(halfWidth, halfHeight),
//         Vec2D(-halfWidth, halfHeight),
//     };
//     m_vertexBuffer = sg_make_buffer((sg_buffer_desc){
//         .type = SG_BUFFERTYPE_VERTEXBUFFER,
//         .data = SG_RANGE(points),
//     });

//     Vec2D uv[] = {
//         uvTransform * Vec2D(0.0f, 0.0f),
//         uvTransform * Vec2D(1.0f, 0.0f),
//         uvTransform * Vec2D(1.0f, 1.0f),
//         uvTransform * Vec2D(0.0f, 1.0f),
//     };

//     m_uvBuffer = sg_make_buffer((sg_buffer_desc){
//         .type = SG_BUFFERTYPE_VERTEXBUFFER,
//         .data = SG_RANGE(uv),
//     });
// }

// SokolRenderImage::~SokolRenderImage() {
//     sg_destroy_buffer(m_vertexBuffer);
//     sg_destroy_buffer(m_uvBuffer);
// }
