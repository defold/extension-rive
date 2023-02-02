// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "render_jni.h"
#include "defold_jni.h"

#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/align.h>
#include <dmsdk/dlib/transform.h>
#include <dmsdk/render/render.h>
#include <dmsdk/graphics/graphics.h>


static void OutputTransform(const dmTransform::Transform& transform)
{
    printf("    t: %f, %f, %f\n", transform.GetTranslation().getX(), transform.GetTranslation().getY(), transform.GetTranslation().getZ());
    printf("    r: %f, %f, %f, %f\n", transform.GetRotation().getX(), transform.GetRotation().getY(), transform.GetRotation().getZ(), transform.GetRotation().getW());
    printf("    s: %f, %f, %f\n", transform.GetScale().getX(), transform.GetScale().getY(), transform.GetScale().getZ());
}

namespace dmRenderJNI
{

// ******************************************************************************************************************


struct ConstantJNI
{
    jclass      cls;
    jfieldID    nameHash;
    jfieldID    values; // array of Vec4
} g_ConstantJNI;

struct StencilTestFuncJNI
{
    jclass      cls;
    jfieldID    func;
    jfieldID    opSFail;
    jfieldID    opDPFail;
    jfieldID    opDPPass;
} g_StencilTestFuncJNI;

struct StencilTestParamsJNI
{
    jclass      cls;
    jfieldID    front;
    jfieldID    back;
    jfieldID    ref;
    jfieldID    refMask;
    jfieldID    bufferMask;
    jfieldID    colorBufferMask;
    jfieldID    clearBuffer;
    jfieldID    separateFaceStates;
} g_StencilTestParamsJNI;

struct RenderObjectJNI
{
    // static const uint32_t MAX_TEXTURE_COUNT = 8;

    jclass      cls;
    jfieldID    constantBuffer;         // HNamedConstantBuffer
    jfieldID    worldTransform;         // dmVMath::Matrix4
    jfieldID    textureTransform;       // dmVMath::Matrix4
    jfieldID    vertexBuffer;           // dmGraphics::HVertexBuffer
    jfieldID    vertexDeclaration;      // dmGraphics::HVertexDeclaration
    jfieldID    indexBuffer;            // dmGraphics::HIndexBuffer
    jfieldID    material;               // HMaterial
    jfieldID    textures;               //[MAX_TEXTURE_COUNT]; // dmGraphics::HTexture
    jfieldID    primitiveType;          // dmGraphics::PrimitiveType
    jfieldID    indexType;              // dmGraphics::Type
    jfieldID    sourceBlendFactor;      // dmGraphics::BlendFactor
    jfieldID    destinationBlendFactor; // dmGraphics::BlendFactor
    jfieldID    faceWinding;            // dmGraphics::FaceWinding
    jfieldID    stencilTestParams;      // StencilTestParams
    jfieldID    vertexStart;            // uint32_t
    jfieldID    vertexCount;            // uint32_t
    jfieldID    setBlendFactors;        // uint8_t :1
    jfieldID    setStencilTest;         // uint8_t :1
    jfieldID    setFaceWinding;         // uint8_t :1
} g_RenderObjectJNI;

void InitializeJNITypes(JNIEnv* env)
{
    #define DM_DEFOLD_JNI_PACKAGE_NAME "com/dynamo/bob/pipeline/DefoldJNI"
    #define DM_RENDER_JNI_PACKAGE_NAME "com/dynamo/bob/pipeline/RenderJNI"
    // https://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/types.html

    {
        SETUP_CLASS(ConstantJNI, MAKE_TYPE_NAME(DM_RENDER_JNI_PACKAGE_NAME, "Constant"));
        GET_FLD_TYPESTR(nameHash, "J");
        GET_FLD_ARRAY(values, MAKE_TYPE_NAME(DM_DEFOLD_JNI_PACKAGE_NAME, "Vec4"));
    }
    {
        SETUP_CLASS(StencilTestFuncJNI, MAKE_TYPE_NAME(DM_RENDER_JNI_PACKAGE_NAME, "StencilTestFunc"));
        GET_FLD_TYPESTR(func, "I");
        GET_FLD_TYPESTR(opSFail, "I");
        GET_FLD_TYPESTR(opDPFail, "I");
        GET_FLD_TYPESTR(opDPPass, "I");
    }

    {
        SETUP_CLASS(StencilTestParamsJNI, MAKE_TYPE_NAME(DM_RENDER_JNI_PACKAGE_NAME, "StencilTestParams"));
        GET_FLD(front, MAKE_TYPE_NAME(DM_RENDER_JNI_PACKAGE_NAME, "StencilTestFunc"));
        GET_FLD(back, MAKE_TYPE_NAME(DM_RENDER_JNI_PACKAGE_NAME, "StencilTestFunc"));
        GET_FLD_TYPESTR(ref, "I");
        GET_FLD_TYPESTR(refMask, "I");
        GET_FLD_TYPESTR(bufferMask, "I");
        GET_FLD_TYPESTR(colorBufferMask, "I");
        GET_FLD_TYPESTR(clearBuffer, "I");
        GET_FLD_TYPESTR(separateFaceStates, "I");
    }
    {
        SETUP_CLASS(RenderObjectJNI, MAKE_TYPE_NAME(DM_RENDER_JNI_PACKAGE_NAME, "RenderObject"));
        GET_FLD_ARRAY(constantBuffer, MAKE_TYPE_NAME(DM_RENDER_JNI_PACKAGE_NAME, "Constant"));

        GET_FLD(worldTransform, MAKE_TYPE_NAME(DM_DEFOLD_JNI_PACKAGE_NAME, "Matrix4"));
        GET_FLD(textureTransform, MAKE_TYPE_NAME(DM_DEFOLD_JNI_PACKAGE_NAME, "Matrix4"));

        GET_FLD_TYPESTR(vertexBuffer, "J");
        GET_FLD_TYPESTR(vertexDeclaration, "J");
        GET_FLD_TYPESTR(indexBuffer, "J");
        GET_FLD_TYPESTR(material, "J");
        GET_FLD_TYPESTR(textures, "[I");
        GET_FLD_TYPESTR(primitiveType, "I");

        GET_FLD_TYPESTR(indexType, "I");              // dmGraphics::Type
        GET_FLD_TYPESTR(sourceBlendFactor, "I");      // dmGraphics::BlendFactor
        GET_FLD_TYPESTR(destinationBlendFactor, "I"); // dmGraphics::BlendFactor
        GET_FLD_TYPESTR(faceWinding, "I");            // dmGraphics::FaceWinding
        GET_FLD_TYPESTR(vertexStart, "I");            // uint32_t
        GET_FLD_TYPESTR(vertexCount, "I");            // uint32_t
        GET_FLD(stencilTestParams, MAKE_TYPE_NAME(DM_RENDER_JNI_PACKAGE_NAME, "StencilTestParams"));      // StencilTestParams

        GET_FLD_TYPESTR(setBlendFactors, "Z");
        GET_FLD_TYPESTR(setStencilTest, "Z");
        GET_FLD_TYPESTR(setFaceWinding, "Z");
    }

    #undef DM_DEFOLD_JNI_PACKAGE_NAME
    #undef DM_RENDER_JNI_PACKAGE_NAME
}


void FinalizeJNITypes(JNIEnv* env)
{
}


// ******************************************************************************************************************

static jobject CreateStencilTestFunc(JNIEnv* env,
                                        dmGraphics::CompareFunc func,
                                        dmGraphics::StencilOp opSFail,
                                        dmGraphics::StencilOp opDPFail,
                                        dmGraphics::StencilOp opDPPass)
{
    jobject obj = env->AllocObject(g_StencilTestFuncJNI.cls);
    env->SetIntField(obj, g_StencilTestFuncJNI.func, (int)func);
    env->SetIntField(obj, g_StencilTestFuncJNI.opSFail, (int)opSFail);
    env->SetIntField(obj, g_StencilTestFuncJNI.opDPFail, (int)opDPFail);
    env->SetIntField(obj, g_StencilTestFuncJNI.opDPPass, (int)opDPPass);
    return obj;
}

static jobject CreateStencilTestParams(JNIEnv* env, const dmRender::StencilTestParams* params)
{
    jobject obj = env->AllocObject(g_StencilTestParamsJNI.cls);
    dmDefoldJNI::SetFieldObject(env, obj, g_StencilTestParamsJNI.front, CreateStencilTestFunc(env, params->m_Front.m_Func, params->m_Front.m_OpSFail, params->m_Front.m_OpDPFail, params->m_Front.m_OpDPPass));
    dmDefoldJNI::SetFieldObject(env, obj, g_StencilTestParamsJNI.back, CreateStencilTestFunc(env, params->m_Back.m_Func, params->m_Back.m_OpSFail, params->m_Back.m_OpDPFail, params->m_Back.m_OpDPPass));
    env->SetIntField(obj, g_StencilTestParamsJNI.ref,               (int)params->m_Ref);
    env->SetIntField(obj, g_StencilTestParamsJNI.refMask,           (int)params->m_RefMask);
    env->SetIntField(obj, g_StencilTestParamsJNI.bufferMask,        (int)params->m_BufferMask);
    env->SetIntField(obj, g_StencilTestParamsJNI.colorBufferMask,   (int)params->m_ColorBufferMask);
    env->SetIntField(obj, g_StencilTestParamsJNI.clearBuffer,       (int)params->m_ClearBuffer);
    env->SetIntField(obj, g_StencilTestParamsJNI.separateFaceStates,(int)params->m_SeparateFaceStates);
    return obj;
}

static jobject CreateConstant(JNIEnv* env, dmhash_t name_hash, uint32_t num_values, const dmVMath::Vector4* values)
{
    jobject obj = env->AllocObject(g_ConstantJNI.cls);
    env->SetLongField(obj, g_ConstantJNI.nameHash, (jlong)name_hash);

    jobjectArray arr = dmDefoldJNI::CreateVec4Array(env, num_values, values);
    dmDefoldJNI::SetFieldObject(env, obj, g_ConstantJNI.values, arr);
    env->DeleteLocalRef(arr);
    return obj;
}

static void IterateNamedConstants(dmhash_t name_hash, void* ctx)
{
    dmArray<dmhash_t>* names = (dmArray<dmhash_t>*)ctx;
    if (names->Full())
        names->OffsetCapacity(8);
    names->Push(name_hash);
}

static jobject CreateNamedConstantBuffer(JNIEnv* env, const dmRender::HNamedConstantBuffer buffer)
{
    dmArray<dmhash_t> names;
    dmRender::IterateNamedConstants(buffer, &IterateNamedConstants, &names);
    uint32_t count = names.Size();

    jobjectArray arr = env->NewObjectArray(count, g_ConstantJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        dmhash_t name_hash = names[i];
        dmVMath::Vector4* values;
        uint32_t num_values;

        if (dmRender::GetNamedConstant(buffer, name_hash, &values, &num_values))
        {
            jobject o = CreateConstant(env, name_hash, num_values, values);
            env->SetObjectArrayElement(arr, i, o);
            env->DeleteLocalRef(o);
        }
    }
    return arr;
}

jobject CreateRenderObject(JNIEnv* env, const dmRender::RenderObject* ro)
{
    jobject obj = env->AllocObject(g_RenderObjectJNI.cls);

    jobject o = CreateNamedConstantBuffer(env, ro->m_ConstantBuffer);
    dmDefoldJNI::SetFieldObject(env, obj, g_RenderObjectJNI.constantBuffer, o);
    env->DeleteLocalRef(o);

    o = dmDefoldJNI::CreateMatrix4(env, &ro->m_WorldTransform);
    dmDefoldJNI::SetFieldObject(env, obj, g_RenderObjectJNI.worldTransform, o);
    env->DeleteLocalRef(o);

    o = dmDefoldJNI::CreateMatrix4(env, &ro->m_TextureTransform);
    dmDefoldJNI::SetFieldObject(env, obj, g_RenderObjectJNI.textureTransform, o);
    env->DeleteLocalRef(o);

    o = CreateStencilTestParams(env, &ro->m_StencilTestParams);
    dmDefoldJNI::SetFieldObject(env, obj, g_RenderObjectJNI.stencilTestParams, o);
    env->DeleteLocalRef(o);

    env->SetLongField(obj, g_RenderObjectJNI.vertexDeclaration, (uintptr_t)ro->m_VertexDeclaration);
    env->SetLongField(obj, g_RenderObjectJNI.vertexBuffer,      (uintptr_t)ro->m_VertexBuffer);
    env->SetLongField(obj, g_RenderObjectJNI.indexBuffer,       (uintptr_t)ro->m_IndexBuffer);
    env->SetLongField(obj, g_RenderObjectJNI.material,          (uintptr_t)ro->m_Material);

    jintArray textures_arr = dmDefoldJNI::CreateIntArray(env, 0, 0);
    env->SetObjectField(obj, g_RenderObjectJNI.textures, textures_arr);
    env->DeleteLocalRef(textures_arr);

    env->SetIntField(obj, g_RenderObjectJNI.primitiveType,      (int)ro->m_PrimitiveType);
    env->SetIntField(obj, g_RenderObjectJNI.indexType,          (int)ro->m_IndexType);
    env->SetIntField(obj, g_RenderObjectJNI.sourceBlendFactor,  (int)ro->m_SourceBlendFactor);
    env->SetIntField(obj, g_RenderObjectJNI.destinationBlendFactor, (int)ro->m_DestinationBlendFactor);
    env->SetIntField(obj, g_RenderObjectJNI.faceWinding,        (int)ro->m_FaceWinding);

    env->SetIntField(obj, g_RenderObjectJNI.vertexStart,        (int)ro->m_VertexStart);
    env->SetIntField(obj, g_RenderObjectJNI.vertexCount,        (int)ro->m_VertexCount);
    env->SetBooleanField(obj, g_RenderObjectJNI.setBlendFactors,    (int)ro->m_SetBlendFactors);
    env->SetBooleanField(obj, g_RenderObjectJNI.setStencilTest,     (int)ro->m_SetStencilTest);
    env->SetBooleanField(obj, g_RenderObjectJNI.setFaceWinding,     (int)ro->m_SetFaceWinding);

    return obj;
}

jobjectArray CreateRenderObjectArray(JNIEnv* env, uint32_t num_values, const dmRender::RenderObject* values)
{
    jobjectArray arr = env->NewObjectArray(num_values, g_RenderObjectJNI.cls, 0);
    for (uint32_t i = 0; i < num_values; ++i)
    {
        jobject o = CreateRenderObject(env, &values[i]);
        env->SetObjectArrayElement(arr, i, o);
        env->DeleteLocalRef(o);
    }
    return arr;
}

} // namespace

