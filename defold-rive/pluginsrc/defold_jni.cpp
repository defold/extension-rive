
#include "defold_jni.h"

#include <dmsdk/dlib/dstrings.h>

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

void CheckJniException(JNIEnv* env, const char* function, int line)
{
    jthrowable throwable = env->ExceptionOccurred();
    if (throwable == NULL)
        return;

    printf("%s:%d: Jni error\n", function, line);
    env->ExceptionDescribe();
    env->ExceptionClear();
}

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
    return env->FindClass(clsname);
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

static jobject NewObject(JNIEnv* env, jclass cls)
{
    jmethodID init = env->GetMethodID(cls, "<init>", "()V");
    return env->NewObject(cls, init);
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

jobjectArray CreateVec4Array(JNIEnv* env, uint32_t num_values, const dmVMath::Vector4* values)
{
    jobjectArray arr = env->NewObjectArray(num_values, g_Vec4JNI.cls, 0);
    for (uint32_t i = 0; i < num_values; ++i)
    {
        jobject o = CreateVec4(env, values[i]);
        env->SetObjectArrayElement(arr, i, o);
        env->DeleteLocalRef(o);
    }
    return arr;
}

jobject CreateAABB(JNIEnv* env, const dmVMath::Vector4& aabb_min, const dmVMath::Vector4& aabb_max)
{
    jobject obj = env->AllocObject(g_AabbJNI.cls);

    jobject min = CreateVec4(env, aabb_min);
    env->SetObjectField(obj, g_AabbJNI.min, min);
    env->DeleteLocalRef(min);

    jobject max = CreateVec4(env, aabb_max);
    env->SetObjectField(obj, g_AabbJNI.max, max);
    env->DeleteLocalRef(max);

    return obj;
}

// For debugging the set values
void GetVec4(JNIEnv* env, jobject object, jfieldID field, dmVMath::Vector4* vec4)
{
    jobject vobject = env->GetObjectField(object, field);
    float v[4];
    v[0] = env->GetFloatField(vobject, g_Vec4JNI.x);
    v[1] = env->GetFloatField(vobject, g_Vec4JNI.y);
    v[2] = env->GetFloatField(vobject, g_Vec4JNI.z);
    v[3] = env->GetFloatField(vobject, g_Vec4JNI.w);
    env->DeleteLocalRef(vobject);
    *vec4 = dmVMath::Vector4(v[0], v[1], v[2], v[3]);
}

// void GetTransform(JNIEnv* env, jobject object, jfieldID field, dmTransform::Transform* out)
// {
//     jobject xform = env->GetObjectField(object, field);
//     // TODO: check if it's a transform class!

//     jobject xform_pos = env->GetObjectField(xform, g_TransformJNI.translation);
//     jobject xform_rot = env->GetObjectField(xform, g_TransformJNI.rotation);
//     jobject xform_scl = env->GetObjectField(xform, g_TransformJNI.scale);

//     float v[4] = {};
//     GetVec4(env, xform_pos, v);
//     out->SetTranslation(dmVMath::Vector3(v[0], v[1], v[2]));
//     GetVec4(env, xform_rot, v);
//     out->SetRotation(dmVMath::Quat(v[0], v[1], v[2], v[3]));
//     GetVec4(env, xform_scl, v);
//     out->SetScale(dmVMath::Vector3(v[0], v[1], v[2]));

//     env->DeleteLocalRef(xform_pos);
//     env->DeleteLocalRef(xform_rot);
//     env->DeleteLocalRef(xform_scl);
//     env->DeleteLocalRef(xform);
// }

void GetMatrix4(JNIEnv* env, jobject object, jfieldID field, dmVMath::Matrix4* out)
{
    jfloatArray mobject = (jfloatArray)env->GetObjectField(object, field);
    float* data = env->GetFloatArrayElements(mobject, NULL);

    float* outf = (float*)out;
    for (int i = 0; i < 16; ++i)
    {
        outf[i] = data[i];
    }

    env->ReleaseFloatArrayElements(mobject, data, 0);
    env->DeleteLocalRef(mobject);
}

} // namespace

