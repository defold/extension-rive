//
// License: MIT
//

package com.dynamo.bob.pipeline;

import com.dynamo.bob.pipeline.DefoldJNI.Vec4;
import com.dynamo.bob.pipeline.DefoldJNI.Matrix4;

public class RenderJNI {

    public static class Constant {
        public long     nameHash;
        public Vec4[]   values;
        // TODO: create a getValuesAsFloats
    }

    public static class StencilTestFunc {
        public int      func;
        public int      opSFail;
        public int      opDPFail;
        public int      opDPPass;
    }

    public static class StencilTestParams {
        public StencilTestFunc  front;
        public StencilTestFunc  back;
        public int              ref;
        public int              refMask;
        public int              bufferMask;
        public int              colorBufferMask;
        public int              clearBuffer;
        public int              separateFaceStates;
    }

    public static class RenderObject {
        public Constant[]   constantBuffer;         // HNamedConstantBuffer
        public Matrix4      worldTransform;         // dmVMath::Matrix4
        public Matrix4      textureTransform;       // dmVMath::Matrix4
        public long         vertexBuffer;           // dmGraphics::HVertexBuffer
        public long         vertexDeclaration;      // dmGraphics::HVertexDeclaration
        public long         indexBuffer;            // dmGraphics::HIndexBuffer
        public long         material;               // HMaterial
        public int[]        textures;               //[MAX_TEXTURE_COUNT]; // dmGraphics::HTexture
        public int          primitiveType;          // dmGraphics::PrimitiveType
        public int          indexType;              // dmGraphics::Type
        public int          sourceBlendFactor;      // dmGraphics::BlendFactor
        public int          destinationBlendFactor; // dmGraphics::BlendFactor
        public int          faceWinding;            // dmGraphics::FaceWinding
        public StencilTestParams stencilTestParams; // StencilTestParams
        public int          vertexStart;            // uint32_t
        public int          vertexCount;            // uint32_t
        public boolean      setBlendFactors;        // uint8_t :1
        public boolean      setStencilTest;         // uint8_t :1
        public boolean      setFaceWinding;         // uint8_t :1
    }
}
