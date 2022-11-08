
#include <stdint.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/graphics/graphics.h>
#include <dmsdk/render/render.h>

#include <rive/math/mat2d.hpp>
#include <rive/renderer.hpp>

// Common source
#include <common/tess_renderer.h>

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
        float u;
        float v;
    };

    struct RiveBuffer
    {
        void*        m_Data;
        unsigned int m_Size;
    };

    void ApplyDrawMode(dmRender::RenderObject& ro, dmRive::DrawMode draw_mode, uint8_t clipIndex);

    void CopyVertices(const dmRive::DrawDescriptor& draw_desc, uint32_t vertex_offset, RiveVertex* out_vertices, uint16_t* out_indices);

    // // We use a simpler version here, since we don't know the location of the variable in the shader
    // struct ShaderConstant
    // {
    //     static const uint32_t MAX_VALUES_COUNT = 32;
    //     dmVMath::Vector4 m_Values[MAX_VALUES_COUNT];
    //     dmhash_t         m_NameHash;
    //     uint32_t         m_Count;
    //     uint8_t          pad[4];
    // };

    // // Due to the fact that the struct has bit fields, it's not possible to get a 1:1 mapping using JNA
    // // See dmsdk/render/render.h for the actual implementation
    // struct StencilTestParams
    // {
    //     void Init();

    //     struct
    //     {
    //         dmGraphics::CompareFunc m_Func;
    //         dmGraphics::StencilOp   m_OpSFail;
    //         dmGraphics::StencilOp   m_OpDPFail;
    //         dmGraphics::StencilOp   m_OpDPPass;
    //     } m_Front;

    //     struct
    //     {
    //         dmGraphics::CompareFunc m_Func;
    //         dmGraphics::StencilOp   m_OpSFail;
    //         dmGraphics::StencilOp   m_OpDPFail;
    //         dmGraphics::StencilOp   m_OpDPPass;
    //     } m_Back;
    //     // 32 bytes
    //     uint8_t m_Ref;
    //     uint8_t m_RefMask;
    //     uint8_t m_BufferMask;
    //     uint8_t m_ColorBufferMask;
    //     bool    m_ClearBuffer;
    //     bool    m_SeparateFaceStates;
    //     uint8_t pad[32 - 6];
    // };

    // // See dmsdk/render/render.h for the actual implementation
    // // similar to dmRender::RenderObject, but used for the Editor
    // // Matches 1:1 with the Java implementation in Rive.java
    // struct RenderObject
    // {
    //     void Init();
    //     void AddConstant(dmhash_t name_hash, const dmVMath::Vector4* values, uint32_t count);

    //     static const uint32_t MAX_CONSTANT_COUNT = 16;

    //     dmRive::StencilTestParams       m_StencilTestParams;
    //     dmVMath::Matrix4                m_WorldTransform;
    //     dmRive::ShaderConstant          m_Constants[MAX_CONSTANT_COUNT];
    //     // 256 bytes
    //     uint32_t                        m_NumConstants;
    //     uint32_t                        m_VertexStart;
    //     uint32_t                        m_VertexCount;
    //     uint32_t                        : 32;

    //     bool                            m_SetBlendFactors;
    //     bool                            m_SetStencilTest;
    //     bool                            m_SetFaceWinding;
    //     bool                            m_FaceWindingCCW;
    //     bool                            m_IsTriangleStrip;
    //     uint8_t                         pad[(4*4) - 5];
    // };

    // Used by both editor and runtime
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
}
