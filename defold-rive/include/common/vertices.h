
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
        dmVMath::Vector4                        m_Value;
        dmhash_t                                m_NameHash;
    };

    struct RenderObject // similar to dmRender::RenderObject, but used for the Editor
    {
        void Init();
        void AddConstant(dmhash_t name_hash, const dmVMath::Vector4& value);

        static const uint32_t MAX_CONSTANT_COUNT = 4;

        ShaderConstant                  m_Constants[MAX_CONSTANT_COUNT];
        uint32_t                        m_NumConstants;

        dmVMath::Matrix4                m_WorldTransform;
        RiveVertex*                     m_VertexBuffer;
        int*                            m_IndexBuffer;
        dmGraphics::PrimitiveType       m_PrimitiveType;
        dmGraphics::Type                m_IndexType;
        dmGraphics::BlendFactor         m_SourceBlendFactor;
        dmGraphics::BlendFactor         m_DestinationBlendFactor;
        dmGraphics::FaceWinding         m_FaceWinding;
        dmRender::StencilTestParams     m_StencilTestParams;
        uint32_t                        m_VertexStart;
        uint32_t                        m_VertexCount;
        uint8_t                         m_SetBlendFactors : 1;
        uint8_t                         m_SetStencilTest : 1;
        uint8_t                         m_SetFaceWinding : 1;
    };

    inline void Mat2DToMat4(const rive::Mat2D m2, dmVMath::Matrix4& m4);

    // Used by both editor and runtime

    void CopyVertices(RiveVertex* dst, const RiveVertex* src, uint32_t count);
    void CopyIndices(int* dst, const int* src, uint32_t count, int index_offset);
    void SetStencilDrawState(dmRender::StencilTestParams* params, bool is_clipping, bool clear_clipping_flag);
    void SetStencilCoverState(dmRender::StencilTestParams* params, bool is_clipping, bool is_applying_clipping);
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

}