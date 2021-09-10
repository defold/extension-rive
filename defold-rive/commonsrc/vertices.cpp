#include <string.h> // memcpy

#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/render/render.h>

#include <common/vertices.h>
#include "rive_ddf.h"

#include <riverender/rive_render_api.h>

namespace dmRive
{
static const dmhash_t UNIFORM_COLOR      = dmHashString64("color");
static const dmhash_t UNIFORM_COVER      = dmHashString64("cover");


void CopyVertices(RiveVertex* dst, const RiveVertex* src, uint32_t count)
{
    memcpy((void*)dst, (void*)src, count*sizeof(RiveVertex));
}

void CopyIndices(int* dst, const int* src, uint32_t count, int index_offset)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        dst[i] = src[i] + index_offset;
    }
}

void GetRiveDrawParams(rive::HContext ctx, rive::HRenderer renderer, uint32_t& vertex_count, uint32_t& index_count, uint32_t& render_object_count)
{
    vertex_count        = 0;
    index_count         = 0;
    render_object_count = 0;

    for (int i = 0; i < rive::getDrawEventCount(renderer); ++i)
    {
        const rive::PathDrawEvent evt = rive::getDrawEvent(renderer, i);

        switch(evt.m_Type)
        {
            case rive::EVENT_DRAW_STENCIL:
            {
                rive::DrawBuffers buffers = rive::getDrawBuffers(ctx, renderer, evt.m_Path);
                RiveBuffer* vxBuffer      = (RiveBuffer*) buffers.m_VertexBuffer;

                if (vxBuffer != 0)
                {
                    uint32_t vx_count = vxBuffer->m_Size / sizeof(RiveVertex);
                    index_count += (vx_count - 5) * 3;
                    vertex_count += vx_count;
                    render_object_count++;
                }
            } break;
            case rive::EVENT_DRAW_COVER:
            {
                rive::DrawBuffers buffers = rive::getDrawBuffers(ctx, renderer, evt.m_Path);
                RiveBuffer* vxBuffer      = (RiveBuffer*) buffers.m_VertexBuffer;

                if (vxBuffer != 0)
                {
                    index_count += 3 * 2;
                    vertex_count += vxBuffer->m_Size / sizeof(RiveVertex);
                    render_object_count++;
                }
            } break;
            default:break;
        }
    }
}

void SetStencilDrawState(dmRender::StencilTestParams* params, bool is_clipping, bool clear_clipping_flag)
{
    params->m_Front = {
        .m_Func     = dmGraphics::COMPARE_FUNC_ALWAYS,
        .m_OpSFail  = dmGraphics::STENCIL_OP_KEEP,
        .m_OpDPFail = dmGraphics::STENCIL_OP_KEEP,
        .m_OpDPPass = dmGraphics::STENCIL_OP_INCR_WRAP,
    };

    params->m_Back = {
        .m_Func     = dmGraphics::COMPARE_FUNC_ALWAYS,
        .m_OpSFail  = dmGraphics::STENCIL_OP_KEEP,
        .m_OpDPFail = dmGraphics::STENCIL_OP_KEEP,
        .m_OpDPPass = dmGraphics::STENCIL_OP_DECR_WRAP,
    };

    params->m_Ref                = 0x00;
    params->m_RefMask            = 0xFF;
    params->m_BufferMask         = 0xFF;
    params->m_ColorBufferMask    = 0x00;
    params->m_ClearBuffer        = 0;
    params->m_SeparateFaceStates = 1;

    if (is_clipping)
    {
        params->m_Front.m_Func = dmGraphics::COMPARE_FUNC_EQUAL;
        params->m_Back.m_Func  = dmGraphics::COMPARE_FUNC_EQUAL;
        params->m_Ref          = 0x80;
        params->m_RefMask      = 0x80;
        params->m_BufferMask   = 0x7F;
    }

    if (clear_clipping_flag)
    {
        params->m_ClearBuffer = 1;
    }
}


void GetBlendFactorsFromBlendMode(dmRiveDDF::RiveModelDesc::BlendMode blend_mode, dmGraphics::BlendFactor* src, dmGraphics::BlendFactor* dst)
{
    switch (blend_mode)
    {
        case dmRiveDDF::RiveModelDesc::BLEND_MODE_ALPHA:
            *src = dmGraphics::BLEND_FACTOR_ONE;
            *dst = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;

        case dmRiveDDF::RiveModelDesc::BLEND_MODE_ADD:
            *src = dmGraphics::BLEND_FACTOR_ONE;
            *dst = dmGraphics::BLEND_FACTOR_ONE;
        break;

        case dmRiveDDF::RiveModelDesc::BLEND_MODE_MULT:
            *src = dmGraphics::BLEND_FACTOR_DST_COLOR;
            *dst = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;

        case dmRiveDDF::RiveModelDesc::BLEND_MODE_SCREEN:
            *src = dmGraphics::BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            *dst = dmGraphics::BLEND_FACTOR_ONE;
        break;

        default:
            dmLogError("Unknown blend mode: %d\n", blend_mode);
            assert(0);
        break;
    }
}

static void SetRenderObject_DrawStencil(
    const rive::PathDrawEvent&  evt,
    bool                        clear_clipping_flag,
    uint32_t                    ix_start, // bytes
    uint32_t                    ix_count,
    // out parameters
    // RiveVertex*                 vx_ptr,
    // int*                        ix_ptr,
    dmRender::RenderObject*     ro)
{
    // rive::DrawBuffers buffers = rive::getDrawBuffers(rive_ctx, renderer, evt.m_Path);
    // RiveBuffer* vxBuffer      = (RiveBuffer*) buffers.m_VertexBuffer;
    // RiveBuffer* ixBuffer      = (RiveBuffer*) buffers.m_IndexBuffer;

    // if (vxBuffer != 0 || ixBuffer)
    // {
    //     return;
    // }

    // uint32_t vx_count = vxBuffer->m_Size / sizeof(RiveVertex);

    // int triangle_count = vx_count - 5;
    // uint32_t ix_count = triangle_count * 3;
    // int* ix_data_ptr = (int*) ixBuffer->m_Data;

    // // Although we can early out on low vertex counts
    // // it's still good to have the indices valid
    // for (int j = 0; j < ix_count; ++j)
    // {
    //     ix_ptr[j] = ix_data_ptr[j + 6] + vx_offset;
    // }

    // if (vx_count < 5)
    // {
    //     return;
    // }

    // memcpy(vx_ptr, vxBuffer->m_Data, vxBuffer->m_Size);

    //dmRender::RenderObject& ro = world->m_RenderObjects[ro_index];
    //ro.Init();
    // ro.m_VertexDeclaration = world->m_VertexDeclaration;
    // ro.m_VertexBuffer      = world->m_VertexBuffer;
    ro->m_PrimitiveType     = dmGraphics::PRIMITIVE_TRIANGLES;
    ro->m_VertexStart       = ix_start; // byte offset
    ro->m_VertexCount       = ix_count;
    //ro->m_Material          = material;
    //ro->m_IndexBuffer       = world->m_IndexBuffer;
    ro->m_IndexType         = dmGraphics::TYPE_UNSIGNED_INT;
    ro->m_SetStencilTest    = 1;
    ro->m_SetFaceWinding    = 1;

    if (evt.m_IsEvenOdd)
    {
        ro->m_FaceWinding = ((evt.m_Idx % 2) != 0) ? dmGraphics::FACE_WINDING_CW : dmGraphics::FACE_WINDING_CCW;
    }
    else
    {
        ro->m_FaceWinding = dmGraphics::FACE_WINDING_CCW;
    }

    SetStencilDrawState(&ro->m_StencilTestParams, evt.m_IsClipping, clear_clipping_flag);

    dmRender::EnableRenderObjectConstant(ro, UNIFORM_COVER, dmVMath::Vector4(0.0f, 0.0f, 0.0f, 0.0f));

    Mat2DToMat4(evt.m_TransformWorld, ro->m_WorldTransform);
}



uint32_t GenerateRenderData(

    // dmGraphics::HVertexDeclaration      vertex_declaration;
    //     dmGraphics::HVertexBuffer           m_VertexBuffer;
    //     dmArray<RiveVertex>                 m_VertexBufferData;
    //     dmGraphics::HIndexBuffer            m_IndexBuffer;

    //dmRender::HRenderContext                render_context,
    //dmRender::HMaterial                     material,
    dmRiveDDF::RiveModelDesc::BlendMode     blend_mode,
    rive::HContext                          rive_ctx,
    rive::HRenderer                         renderer,
    RiveVertex*                             vx_ptr,
    int*                                    ix_ptr,
    RenderObject*                           render_objects, uint32_t ro_count)
{
    dmGraphics::FaceWinding last_face_winding = dmGraphics::FACE_WINDING_CCW;
    rive::HRenderPaint paint                  = 0;
    uint32_t last_ix                          = 0;
    uint32_t vx_offset                        = 0;
    uint8_t clear_clipping_flag               = 0;
    bool is_applying_clipping                 = false;
    uint32_t ro_index                         = 0;

    // The index buffer is used for all render paths (as it's generic)
    // The first 6 indices forms are used for a quad for the Cover case. (i.e. 0,1,2,2,3,0)
    // The remaining indices form a triangle list for the Stencil case
    rive::DrawBuffers buffers_renderer = rive::getDrawBuffers(rive_ctx, renderer, 0);
    RiveBuffer* ixBuffer               = (RiveBuffer*) buffers_renderer.m_IndexBuffer;

    if (!ixBuffer)
    {
        return ro_index;
    }

    int* ix_data_ptr = (int*) ixBuffer->m_Data;

    for (int i = 0; i < rive::getDrawEventCount(renderer); ++i)
    {
        const rive::PathDrawEvent evt = rive::getDrawEvent(renderer, i);

        switch(evt.m_Type)
        {
            case rive::EVENT_SET_PAINT:
                paint = evt.m_Paint;
                break;
            case rive::EVENT_DRAW_STENCIL:
            {
                rive::DrawBuffers buffers = rive::getDrawBuffers(rive_ctx, renderer, evt.m_Path);
                RiveBuffer* vxBuffer      = (RiveBuffer*) buffers.m_VertexBuffer;
                if (!vxBuffer)
                {
                    continue;
                }

                uint32_t vx_count = vxBuffer->m_Size / sizeof(RiveVertex);
                int triangle_count = vx_count - 5;
                uint32_t ix_count = triangle_count * 3;

                CopyVertices(vx_ptr, (RiveVertex*)vxBuffer->m_Data, vx_count);
                CopyIndices(ix_ptr, ix_data_ptr+6, ix_count, vx_offset);

                // Although we can early out on low vertex counts
                // it's still good to have the indices valid
                if (vx_count < 5)
                {
                    continue;
                }

                //dmRender::RenderObject& ro = render_objects[ro_index++];

                //dmRender::RenderObject& ro = world->m_RenderObjects[ro_index];
                dmRender::RenderObject ro;

                ro.Init();
                // ro.m_VertexDeclaration = world->m_VertexDeclaration;
                // ro.m_VertexBuffer      = world->m_VertexBuffer;
                // ro.m_IndexBuffer       = world->m_IndexBuffer;
                // ro.m_Material          = material;

                //SetRenderObject_DrawStencil(evt, clear_clipping_flag, last_ix, ix_count, &ro);

                //dmRender::AddToRender(render_context, &ro);

                last_face_winding  = ro.m_FaceWinding;
                vx_ptr            += vx_count;
                ix_ptr            += ix_count;
                last_ix           += ix_count * sizeof(int);
                vx_offset         += vx_count;
            } break;
            case rive::EVENT_DRAW_COVER:
            {
                rive::DrawBuffers buffers = rive::getDrawBuffers(rive_ctx, renderer, evt.m_Path);
                RiveBuffer* vxBuffer      = (RiveBuffer*) buffers.m_VertexBuffer;

                if (vxBuffer != 0)
                {
                    uint32_t vx_count = 4;
                    uint32_t ix_count = 2 * 3;

                    // for (int j = 0; j < ix_count; ++j)
                    // {
                    //     ix_ptr[j] = ix_data_ptr[j] + vx_offset;
                    // }

                    // memcpy(vx_ptr, vxBuffer->m_Data, vxBuffer->m_Size);

                    CopyVertices(vx_ptr, (RiveVertex*)vxBuffer->m_Data, vx_count);
                    CopyIndices(ix_ptr, ix_data_ptr, ix_count, vx_offset);

                    //dmRender::RenderObject& ro = render_objects[ro_index++];
                    dmRender::RenderObject ro;

                    ro.Init();
                    // ro.m_VertexDeclaration = world->m_VertexDeclaration;
                    // ro.m_VertexBuffer      = world->m_VertexBuffer;
                    ro.m_PrimitiveType     = dmGraphics::PRIMITIVE_TRIANGLES;
                    ro.m_VertexStart       = last_ix; // byte offset
                    ro.m_VertexCount       = ix_count;
                    // ro.m_Material          = material;
                    // ro.m_IndexBuffer       = world->m_IndexBuffer;
                    ro.m_IndexType         = dmGraphics::TYPE_UNSIGNED_INT;
                    ro.m_SetStencilTest    = 1;

                    dmRender::StencilTestParams& stencil_state = ro.m_StencilTestParams;
                    stencil_state.m_ClearBuffer = 0;

                    if (is_applying_clipping)
                    {
                        stencil_state.m_Front = {
                            .m_Func     = dmGraphics::COMPARE_FUNC_NOTEQUAL,
                            .m_OpSFail  = dmGraphics::STENCIL_OP_ZERO,
                            .m_OpDPFail = dmGraphics::STENCIL_OP_ZERO,
                            .m_OpDPPass = dmGraphics::STENCIL_OP_REPLACE,
                        };

                        stencil_state.m_Ref             = 0x80;
                        stencil_state.m_RefMask         = 0x7F;
                        stencil_state.m_BufferMask      = 0xFF;
                        stencil_state.m_ColorBufferMask = 0x00;
                    }
                    else
                    {
                        stencil_state.m_Front = {
                            .m_Func     = dmGraphics::COMPARE_FUNC_NOTEQUAL,
                            .m_OpSFail  = dmGraphics::STENCIL_OP_ZERO,
                            .m_OpDPFail = dmGraphics::STENCIL_OP_ZERO,
                            .m_OpDPPass = dmGraphics::STENCIL_OP_ZERO,
                        };

                        stencil_state.m_Ref             = 0x00;
                        stencil_state.m_RefMask         = 0xFF;
                        stencil_state.m_BufferMask      = 0xFF;
                        stencil_state.m_ColorBufferMask = 0x0F;

                        if (evt.m_IsClipping)
                        {
                            stencil_state.m_RefMask    = 0x7F;
                            stencil_state.m_BufferMask = 0x7F;
                        }

                        GetBlendFactorsFromBlendMode(blend_mode, &ro.m_SourceBlendFactor, &ro.m_DestinationBlendFactor);
                        ro.m_SetBlendFactors = 1;

                        const rive::PaintData draw_entry_paint = rive::getPaintData(paint);
                        const float* color                     = &draw_entry_paint.m_Colors[0];

                        dmRender::EnableRenderObjectConstant(&ro, UNIFORM_COLOR, dmVMath::Vector4(color[0], color[1], color[2], color[3]));

                    }

                    // If we are fullscreen-covering, we don't transform the vertices
                    float no_projection = (float) evt.m_IsClipping && is_applying_clipping;

                    dmRender::EnableRenderObjectConstant(&ro, UNIFORM_COVER, dmVMath::Vector4(no_projection, 0.0f, 0.0f, 0.0f));

                    if (last_face_winding != dmGraphics::FACE_WINDING_CCW)
                    {
                        ro.m_FaceWinding    = dmGraphics::FACE_WINDING_CCW;
                        ro.m_SetFaceWinding = 1;
                    }

                    Mat2DToMat4(evt.m_TransformWorld, ro.m_WorldTransform);
                    //dmRender::AddToRender(render_context, &ro);

                    vx_ptr    += vx_count;
                    ix_ptr    += ix_count;
                    last_ix   += ix_count * sizeof(int);
                    vx_offset += vx_count;
                    //ro_index++;
                }
            } break;
            case rive::EVENT_CLIPPING_BEGIN:
                is_applying_clipping = true;
                clear_clipping_flag = 1;
                break;
            case rive::EVENT_CLIPPING_END:
                is_applying_clipping = false;
                break;
            case rive::EVENT_CLIPPING_DISABLE:
                is_applying_clipping = false;
                break;
            default:break;
        }

        return ro_index;
    }
}

} // namespace
