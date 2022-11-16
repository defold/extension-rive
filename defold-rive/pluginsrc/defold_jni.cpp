
#include "defold_jni.h"

#include <dmsdk/dlib/dstrings.h>

#include <jni.h>

namespace dmDefoldJNI
{

struct Vec4JNI
{
    jclass      cls;
    jfieldID    x, y, z, w;
} g_Vec4JNI;

struct Matrix4JNI
{
    jclass      cls;
    jfieldID    m; // float[16]
} g_Matrix4JNI;

struct AabbJNI
{
    jclass      cls;
    jfieldID    min;
    jfieldID    max;
} g_AabbJNI;


void InitializeJNITypes(JNIEnv* env)
{
    #define DM_DEFOLD_JNI_PACKAGE_NAME "com/dynamo/bob/pipeline/DefoldJNI"
    {
        SETUP_CLASS(Vec4JNI, MAKE_TYPE_NAME(DM_DEFOLD_JNI_PACKAGE_NAME, "Vec4"));
        GET_FLD_TYPESTR(x, "F");
        GET_FLD_TYPESTR(y, "F");
        GET_FLD_TYPESTR(z, "F");
        GET_FLD_TYPESTR(w, "F");
    }
    {
        SETUP_CLASS(Matrix4JNI, MAKE_TYPE_NAME(DM_DEFOLD_JNI_PACKAGE_NAME, "Matrix4"));
        GET_FLD_TYPESTR(m, "[F");
    }
    {
        SETUP_CLASS(AabbJNI, MAKE_TYPE_NAME(DM_DEFOLD_JNI_PACKAGE_NAME, "Aabb"));
        GET_FLD(min, MAKE_TYPE_NAME(DM_DEFOLD_JNI_PACKAGE_NAME, "Vec4"));
        GET_FLD(max, MAKE_TYPE_NAME(DM_DEFOLD_JNI_PACKAGE_NAME, "Vec4"));
    }
    #undef DM_DEFOLD_JNI_PACKAGE_NAME
}

void FinalizeJNITypes(JNIEnv* env)
{

}

jclass GetVec4JNIClass()
{
    return g_Vec4JNI.cls;
}

// int AddressOf(jobject object)
// {
//     uint64_t a = *(uint64_t*)(uintptr_t)object;
//     return a;
// }

void SetFieldString(JNIEnv* env, jclass cls, jobject obj, const char* field_name, const char* value)
{
    jfieldID field = GetFieldString(env, cls, field_name);
    jstring str = env->NewStringUTF(value);
    env->SetObjectField(obj, field, str);
    env->DeleteLocalRef(str);
}

void SetFieldInt(JNIEnv* env, jobject obj, jfieldID field, int value)
{
    env->SetIntField(obj, field, value);
}
void SetFieldFloat(JNIEnv* env, jobject obj, jfieldID field, float value)
{
    env->SetFloatField(obj, field, value);
}
void SetFieldString(JNIEnv* env, jobject obj, jfieldID field, const char* value)
{
    jstring str = env->NewStringUTF(value);
    env->SetObjectField(obj, field, str);
    env->DeleteLocalRef(str);
}
void SetFieldObject(JNIEnv* env, jobject obj, jfieldID field, jobject value)
{
    env->SetObjectField(obj, field, value);
}


jclass GetClass(JNIEnv* env, const char* clsname)
{
    char buffer[128];
    dmSnPrintf(buffer, sizeof(buffer), "L%s;", clsname);
    return env->FindClass(buffer);
}

jfieldID GetFieldInt(JNIEnv* env, jclass cls, const char* field_name)
{
    return env->GetFieldID(cls, field_name, "I");
}
jfieldID GetFieldString(JNIEnv* env, jclass cls, const char* field_name)
{
    return env->GetFieldID(cls, field_name, "Ljava/lang/String;");
}
jfieldID GetFieldOfType(JNIEnv* env, jclass cls, const char* field_name, const char* clsname)
{
    char buffer[128];
    dmSnPrintf(buffer, sizeof(buffer), "L%s;", clsname);
    return env->GetFieldID(cls, field_name, buffer);
}

jintArray CreateIntArray(JNIEnv* env, uint32_t count, const int* values)
{
    jintArray arr = env->NewIntArray(count);
    env->SetIntArrayRegion(arr, 0, count, (const jint*)values);
    return arr;
}

jfloatArray CreateFloatArray(JNIEnv* env, uint32_t count, const float* values)
{
    jfloatArray arr = env->NewFloatArray(count);
    env->SetFloatArrayRegion(arr, 0, count, values);
    return arr;
}

jobjectArray CreateObjectArray(JNIEnv* env, jclass cls, const dmArray<jobject>& values)
{
    uint32_t count = values.Size();
    jobjectArray arr = env->NewObjectArray(count, cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        env->SetObjectArrayElement(arr, i, values[i]);
    }
    return arr;
}

jobject CreateVec4(JNIEnv* env, const dmVMath::Vector4& value)
{
    jobject obj = env->AllocObject(g_Vec4JNI.cls);
    env->SetFloatField(obj, g_Vec4JNI.x, value.getX());
    env->SetFloatField(obj, g_Vec4JNI.y, value.getY());
    env->SetFloatField(obj, g_Vec4JNI.z, value.getZ());
    env->SetFloatField(obj, g_Vec4JNI.w, value.getW());
    return obj;
}

jobject CreateMatrix4(JNIEnv* env, const dmVMath::Matrix4* matrix)
{
    jobject obj = env->AllocObject(g_Matrix4JNI.cls);
    float* values = (float*)matrix;
    jfloatArray arr = dmDefoldJNI::CreateFloatArray(env, 16, values);
    env->SetObjectField(obj, g_Matrix4JNI.m, arr);
    env->DeleteLocalRef(arr);
    return obj;
}

    jobject CreateVec4(JNIEnv* env, const dmVMath::Vector4& value);
    jobject CreateMatrix4(JNIEnv* env, const dmVMath::Matrix4* matrix);
    jobject CreateAABB(JNIEnv* env, const float* aabb_min, const float* aabb_max);

}
