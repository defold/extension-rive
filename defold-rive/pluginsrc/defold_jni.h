
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

#ifndef DM_DEFOLD_JNI_H
#define DM_DEFOLD_JNI_H

#include <dmsdk/dlib/shared_library.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/vmath.h>

#include <stdint.h>
#include "jni/jni.h" // jobject

namespace dmDefoldJNI
{
    void InitializeJNITypes(JNIEnv* env);
    void FinalizeJNITypes(JNIEnv* env);

    jintArray CreateIntArray(JNIEnv* env, uint32_t count, const int* values);
    jfloatArray CreateFloatArray(JNIEnv* env, uint32_t count, const float* values);
    jobjectArray CreateObjectArray(JNIEnv* env, jclass cls, const dmArray<jobject>& values);

    jobject CreateVec4(JNIEnv* env, const dmVMath::Vector4& value);
    jobject CreateMatrix4(JNIEnv* env, const dmVMath::Matrix4* matrix);
    jobject CreateAABB(JNIEnv* env, const dmVMath::Vector4& aabb_min, const dmVMath::Vector4& aabb_max);
    jobjectArray CreateVec4Array(JNIEnv* env, uint32_t num_values, const dmVMath::Vector4* values);

    void SetFieldString(JNIEnv* env, jclass cls, jobject obj, const char* field_name, const char* value);
    void SetFieldInt(JNIEnv* env, jobject obj, jfieldID field, int value);
    void SetFieldFloat(JNIEnv* env, jobject obj, jfieldID field, float value);
    void SetFieldString(JNIEnv* env, jobject obj, jfieldID field, const char* value);
    void SetFieldObject(JNIEnv* env, jobject obj, jfieldID field, jobject value);

    jclass GetClass(JNIEnv* env, const char* clsname); // gets "Lclsname;"
    jfieldID GetFieldInt(JNIEnv* env, jclass cls, const char* field_name);
    jfieldID GetFieldString(JNIEnv* env, jclass cls, const char* field_name);
    jfieldID GetFieldOfType(JNIEnv* env, jclass cls, const char* field_name, const char* clsname); // gets "Lclsname;"

    void GetVec4(JNIEnv* env, jobject object, jfieldID field, dmVMath::Vector4* vec4);
    void GetMatrix4(JNIEnv* env, jobject object, jfieldID field, dmVMath::Matrix4* out);

    void CheckJniException(JNIEnv* env, const char* function, int line);

#define DM_CHECK_JNI_ERROR() dmDefoldJNI::CheckJniException(env, __FUNCTION__, __LINE__)

#define MAKE_TYPE_NAME(PACKAGE_NAME, TYPE_NAME) PACKAGE_NAME "$" TYPE_NAME

#define SETUP_CLASS(TYPE, TYPE_NAME) \
    TYPE * obj = &g_ ## TYPE ; \
    obj->cls = dmDefoldJNI::GetClass(env, TYPE_NAME);

#define GET_FLD_TYPESTR(NAME, FULL_TYPE_STR) \
    obj-> NAME = env->GetFieldID(obj->cls, # NAME, FULL_TYPE_STR);

#define GET_FLD(NAME, TYPE_NAME) \
    obj-> NAME = env->GetFieldID(obj->cls, # NAME, "L" TYPE_NAME ";");

#define GET_FLD_STRING(NAME) \
    obj-> NAME = dmDefoldJNI::GetFieldString(env, obj->cls, # NAME);

#define GET_FLD_ARRAY(NAME, TYPE_NAME) \
    obj-> NAME = env->GetFieldID(obj->cls, # NAME, "[L" TYPE_NAME ";");

    struct ScopedString
    {
        JNIEnv* m_Env;
        jstring m_JString;
        const char* m_String;
        ScopedString(JNIEnv* env, jstring str)
        : m_Env(env)
        , m_JString(str)
        , m_String(env->GetStringUTFChars(str, JNI_FALSE))
        {
        }
        ~ScopedString()
        {
            if (m_String)
            {
                m_Env->ReleaseStringUTFChars(m_JString, m_String);
            }
        }
    };

}

#endif
