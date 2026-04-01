

#include <dmsdk/dlib/vmath.h>
#include <rive/math/mat2d.hpp>

namespace dmRive
{
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

        // This should be the translation
        m2[4] = m4[3][0];
        m2[5] = m4[3][1];
    }
}
