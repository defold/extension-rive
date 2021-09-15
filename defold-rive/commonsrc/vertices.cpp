#include <string.h> // memcpy

#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/render/render.h>

#include <common/vertices.h>

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

void SetStencilCoverState(dmRender::StencilTestParams* params, bool is_clipping, bool is_applying_clipping)
{
    params->m_ClearBuffer = 0;

    if (is_applying_clipping)
    {
        params->m_Front = {
            .m_Func     = dmGraphics::COMPARE_FUNC_NOTEQUAL,
            .m_OpSFail  = dmGraphics::STENCIL_OP_ZERO,
            .m_OpDPFail = dmGraphics::STENCIL_OP_ZERO,
            .m_OpDPPass = dmGraphics::STENCIL_OP_REPLACE,
        };

        params->m_Ref             = 0x80;
        params->m_RefMask         = 0x7F;
        params->m_BufferMask      = 0xFF;
        params->m_ColorBufferMask = 0x00;
    }
    else
    {
        params->m_Front = {
            .m_Func     = dmGraphics::COMPARE_FUNC_NOTEQUAL,
            .m_OpSFail  = dmGraphics::STENCIL_OP_ZERO,
            .m_OpDPFail = dmGraphics::STENCIL_OP_ZERO,
            .m_OpDPPass = dmGraphics::STENCIL_OP_ZERO,
        };

        params->m_Ref             = 0x00;
        params->m_RefMask         = 0xFF;
        params->m_BufferMask      = 0xFF;
        params->m_ColorBufferMask = 0x0F;

        if (is_clipping)
        {
            params->m_RefMask    = 0x7F;
            params->m_BufferMask = 0x7F;
        }
    }
}

uint32_t ProcessRiveEvents(rive::HContext ctx, rive::HRenderer renderer, RiveVertex* vx_ptr, int* ix_ptr,
                            FRiveEventCallback callback, void* user_ctx)
{

    dmGraphics::FaceWinding last_face_winding = dmGraphics::FACE_WINDING_CCW;
    uint32_t last_ix                          = 0;
    uint32_t vx_offset                        = 0;
    uint8_t clear_clipping_flag               = 0;
    bool is_applying_clipping                 = false;
    uint32_t ro_index                         = 0;

    // The index buffer is used for all render paths (as it's generic)
    // The first 6 indices forms are used for a quad for the Cover case. (i.e. 0,1,2,2,3,0)
    // The remaining indices form a triangle list for the Stencil case
    rive::DrawBuffers buffers_renderer = rive::getDrawBuffers(ctx, renderer, 0);
    RiveBuffer* ixBuffer               = (RiveBuffer*) buffers_renderer.m_IndexBuffer;

    if (!ixBuffer)
    {
        dmLogWarning("No index buffer!");
        return ro_index;
    }

    RiveEventsContext cbk_ctx;
    memset(&cbk_ctx, 0, sizeof(cbk_ctx));
    cbk_ctx.m_UserContext = user_ctx;

    cbk_ctx.m_Ctx = ctx;
    cbk_ctx.m_Renderer = renderer;

    const int* ix_data_ptr = (const int*) ixBuffer->m_Data;

    int event_count = rive::getDrawEventCount(renderer);

    for (int i = 0; i < event_count; ++i)
    {
        const rive::PathDrawEvent evt = rive::getDrawEvent(renderer, i);

        cbk_ctx.m_Event = evt;

        switch(evt.m_Type)
        {
            case rive::EVENT_SET_PAINT:
                cbk_ctx.m_Paint = evt.m_Paint;
                break;
            case rive::EVENT_DRAW_STENCIL:
            {
                rive::DrawBuffers buffers = rive::getDrawBuffers(ctx, renderer, evt.m_Path);
                RiveBuffer* vxBuffer      = (RiveBuffer*) buffers.m_VertexBuffer;

                if (vxBuffer == 0)
                {
                    continue;
                }

                uint32_t vx_count = vxBuffer->m_Size / sizeof(RiveVertex);

                int triangle_count = vx_count - 5;
                uint32_t ix_count = triangle_count * 3;

                if (vx_count < 5)
                {
                    continue;
                }

                CopyVertices(vx_ptr, (RiveVertex*)vxBuffer->m_Data, vx_count);
                CopyIndices(ix_ptr, ix_data_ptr+6, ix_count, vx_offset);

                cbk_ctx.m_Index = ro_index++;
                cbk_ctx.m_ClearClippingFlag = clear_clipping_flag;
                cbk_ctx.m_IsApplyingClipping = is_applying_clipping;

                if (evt.m_IsEvenOdd)
                {
                    cbk_ctx.m_FaceWinding = ((evt.m_Idx % 2) != 0) ? dmGraphics::FACE_WINDING_CW : dmGraphics::FACE_WINDING_CCW;
                }
                else
                {
                    cbk_ctx.m_FaceWinding = dmGraphics::FACE_WINDING_CCW;
                }


                cbk_ctx.m_IndexOffsetBytes = last_ix;
                cbk_ctx.m_IndexCount = ix_count;

                callback(&cbk_ctx);

                last_face_winding = cbk_ctx.m_FaceWinding;
                vx_ptr            += vx_count;
                ix_ptr            += ix_count;
                last_ix           += ix_count * sizeof(int);
                vx_offset         += vx_count;

                clear_clipping_flag = 0;

            } break;
            case rive::EVENT_DRAW_COVER:
            {
                rive::DrawBuffers buffers = rive::getDrawBuffers(ctx, renderer, evt.m_Path);
                RiveBuffer* vxBuffer      = (RiveBuffer*) buffers.m_VertexBuffer;

                if ( vxBuffer == 0 || ixBuffer == 0)
                {
                    continue;
                }

                // It's a quad, consisting of two triangles
                int* ix_data_ptr  = (int*) ixBuffer->m_Data;
                uint32_t vx_count = 4;
                uint32_t ix_count = 2 * 3;

                CopyVertices(vx_ptr, (RiveVertex*)vxBuffer->m_Data, vx_count);
                CopyIndices(ix_ptr, ix_data_ptr, ix_count, vx_offset);

                cbk_ctx.m_Index = ro_index++;
                cbk_ctx.m_IndexOffsetBytes = last_ix;
                cbk_ctx.m_IndexCount = ix_count;

                cbk_ctx.m_FaceWinding = dmGraphics::FACE_WINDING_CCW;

                callback(&cbk_ctx);

                vx_ptr    += vx_count;
                ix_ptr    += ix_count;
                last_ix   += ix_count * sizeof(int);
                vx_offset += vx_count;

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
            default:
                break;
        }
    }
    return ro_index;
}


} // namespace
