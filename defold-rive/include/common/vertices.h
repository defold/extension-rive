
#include <stdint.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/graphics/graphics.h>
#include <dmsdk/render/render.h>

#include <rive/math/mat2d.hpp>
#include <riverender/rive_render_api.h>

namespace rive
{
    class Renderer;
    typedef uintptr_t    HContext;
    typedef Renderer*    HRenderer;
}

namespace dmRive
{
    struct RiveVertex
    {
        float x;
        float y;
    };

    struct RiveBuffer
    {
        void*        m_Data;
        unsigned int m_Size;
    };

    // We use a simpler version here, since we don't know the location of the variable in the shader
    struct ShaderConstant
    {
        dmVMath::Vector4 m_Value;
        dmhash_t         m_NameHash;
        uint8_t          pad[8];
    };

    // Due to the fact that the struct has bit fields, it's not possible to get a 1:1 mapping using JNA
    // See dmsdk/render/render.h for the actual implementation
    struct StencilTestParams
    {
        void Init();

        struct
        {
            dmGraphics::CompareFunc m_Func;
            dmGraphics::StencilOp   m_OpSFail;
            dmGraphics::StencilOp   m_OpDPFail;
            dmGraphics::StencilOp   m_OpDPPass;
        } m_Front;

        struct
        {
            dmGraphics::CompareFunc m_Func;
            dmGraphics::StencilOp   m_OpSFail;
            dmGraphics::StencilOp   m_OpDPFail;
            dmGraphics::StencilOp   m_OpDPPass;
        } m_Back;
        // 32 bytes
        uint8_t m_Ref;
        uint8_t m_RefMask;
        uint8_t m_BufferMask;
        uint8_t m_ColorBufferMask;
        bool    m_ClearBuffer;
        bool    m_SeparateFaceStates;
        uint8_t pad[32 - 6];
    };

    // See dmsdk/render/render.h for the actual implementation
    // similar to dmRender::RenderObject, but used for the Editor
    // Matches 1:1 with the Java implementation in Rive.java
    struct RenderObject
    {
        void Init();
        void AddConstant(dmhash_t name_hash, const dmVMath::Vector4& value);

        static const uint32_t MAX_CONSTANT_COUNT = 4;

        dmRive::StencilTestParams       m_StencilTestParams;
        dmVMath::Matrix4                m_WorldTransform;
        dmRive::ShaderConstant          m_Constants[MAX_CONSTANT_COUNT];
        // 256 bytes
        uint32_t                        m_NumConstants;
        uint32_t                        m_VertexStart;
        uint32_t                        m_VertexCount;
        uint32_t                        : 32;

        bool                            m_SetBlendFactors;
        bool                            m_SetStencilTest;
        bool                            m_SetFaceWinding;
        bool                            m_FaceWindingCCW;
        bool                            m_IsTriangleStrip;
        uint8_t                         pad[(4*4) - 5];
    };

    // Used by both editor and runtime
    inline void Mat2DToMat4(const rive::Mat2D m2, dmVMath::Matrix4& m4);

    rive::HBuffer   RequestBufferCallback(rive::HBuffer buffer, rive::BufferType type, void* data, unsigned int dataSize, void* userData);
    void            DestroyBufferCallback(rive::HBuffer buffer, void* userData);

    void CopyVertices(RiveVertex* dst, const RiveVertex* src, uint32_t count);
    void CopyIndices(int* dst, const int* src, uint32_t count, int index_offset);
    void GetRiveDrawParams(rive::HContext ctx, rive::HRenderer renderer, uint32_t& vertex_count, uint32_t& index_count, uint32_t& render_object_count);

    // Used when processing the events
    struct RiveEventsContext
    {
        void*                   m_UserContext; // the context passed into ProcessRiveEvents

        rive::HContext          m_Ctx;
        rive::HRenderer         m_Renderer;
        rive::PathDrawEvent     m_Event;

        rive::HRenderPaint      m_Paint;
        uint32_t                m_Index;

        bool                    m_ClearClippingFlag;
        bool                    m_IsApplyingClipping;

        uint32_t                m_IndexOffsetBytes;
        uint32_t                m_IndexCount;

        dmGraphics::FaceWinding m_FaceWinding;
    };

    typedef void (*FRiveEventCallback)(RiveEventsContext* ctx);

    uint32_t ProcessRiveEvents(rive::HContext ctx, rive::HRenderer renderer, RiveVertex* vx_ptr, int* ix_ptr,
                                FRiveEventCallback callback, void* user_ctx);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    inline void Mat2DToMat4(const rive::Mat2D m2, dmVMath::Matrix4& m4)
    {
        m4[0][0] = m2[0];
        m4[0][1] = m2[1];
        m4[0][2] = 0.0;
        m4[0][3] = 0.0;

        m4[1][0] = m2[2];
        m4[1][1] = m2[3];
        m4[1][2] = 0.0;
        m4[1][3] = 0.0;

        m4[2][0] = 0.0;
        m4[2][1] = 0.0;
        m4[2][2] = 1.0;
        m4[2][3] = 0.0;

        m4[3][0] = m2[4];
        m4[3][1] = m2[5];
        m4[3][2] = 0.0;
        m4[3][3] = 1.0;
    }

    inline void Mat4ToMat2D(const dmVMath::Matrix4& m4, rive::Mat2D& m2)
    {
        m2[0] = m4[0][0];
        m2[1] = m4[0][1];

        m2[2] = m4[1][0];
        m2[3] = m4[1][1];

        m2[4] = m4[3][0];
        m2[5] = m4[3][1];
    }

    template<typename T>
    void SetStencilDrawState(T* params, bool is_clipping, bool clear_clipping_flag)
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

    template<typename T>
    void SetStencilCoverState(T* params, bool is_clipping, bool is_applying_clipping)
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
}