
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
    static const dmhash_t UNIFORM_COLOR           = dmHashString64("colors");
    static const dmhash_t UNIFORM_STOPS           = dmHashString64("stops");
    static const dmhash_t UNIFORM_GRADIENT_LIMITS = dmHashString64("gradientLimits");
    static const dmhash_t UNIFORM_PROPERTIES      = dmHashString64("properties");

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
