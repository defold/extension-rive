

// Fix for "error: undefined symbol: __declspec(dllimport) longjmp" from libtess2
#if defined(_MSC_VER)
#include <setjmp.h>
static jmp_buf jmp_buffer;
__declspec(dllexport) int dummyFunc()
{
    int r = setjmp(jmp_buffer);
    if (r == 0) longjmp(jmp_buffer, 1);
    return r;
}
#endif


// Rive includes
#include <rive/artboard.hpp>
#include <rive/file.hpp>
#include <rive/file_asset_resolver.hpp>
// Due to an X11.h issue (Likely Ubuntu 16.04 issue) we include the Rive/C++17 includes first

//#include <dmsdk/sdk.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/shared_library.h>
#include <dmsdk/dlib/static_assert.h>
#include <stdio.h>
#include <stdint.h>

#include <common/factory.h>
#include <common/atlas.h>
#include <common/bones.h>
#include <common/vertices.h>
#include <common/tess_renderer.h>

#include "jni/jni.h"

#include "defold_jni.h"
#include "render_jni.h"
#include "rive_jni.h"


// Need to free() the buffer
static uint8_t* ReadFile(const char* path, size_t* file_size)
{
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        dmLogError("Failed to read file '%s'", path);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long _file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t* buffer = (uint8_t*)malloc(_file_size);
    if (fread(buffer, 1, _file_size, f) != (size_t) _file_size)
    {
        fclose(f);
        free(buffer);
        return 0;
    }
    fclose(f);

    if (file_size)
        *file_size = _file_size;
    return buffer;
}

// https://android.googlesource.com/platform/libnativehelper/+/b53ec15/JNIHelp.c
// static void getExceptionSummary(JNIEnv* env, jthrowable exception, char* buf, size_t bufLen)
// {
//     int success = 0;
//     /* get the name of the exception's class */
//     jclass exceptionClazz = env->GetObjectClass(exception); // can't fail
//     jclass classClazz = env->GetObjectClass(exceptionClazz); // java.lang.Class, can't fail
//     jmethodID classGetNameMethod = env->GetMethodID(classClazz, "getName", "()Ljava/lang/String;");
//     jstring classNameStr = (jstring)env->CallObjectMethod(exceptionClazz, classGetNameMethod);
//     if (classNameStr != NULL) {
//         /* get printable string */
//         const char* classNameChars = env->GetStringUTFChars(classNameStr, NULL);
//         if (classNameChars != NULL) {
//             /* if the exception has a message string, get that */
//             jmethodID throwableGetMessageMethod = env->GetMethodID(exceptionClazz, "getMessage", "()Ljava/lang/String;");
//             jstring messageStr = (jstring)env->CallObjectMethod(exception, throwableGetMessageMethod);
//             if (messageStr != NULL) {
//                 const char* messageChars = env->GetStringUTFChars(messageStr, NULL);
//                 if (messageChars != NULL) {
//                     snprintf(buf, bufLen, "%s: %s", classNameChars, messageChars);
//                     env->ReleaseStringUTFChars(messageStr, messageChars);
//                 } else {
//                     env->ExceptionClear(); // clear OOM
//                     snprintf(buf, bufLen, "%s: <error getting message>", classNameChars);
//                 }
//                 env->DeleteLocalRef(messageStr);
//             } else {
//                 strncpy(buf, classNameChars, bufLen);
//                 buf[bufLen - 1] = '\0';
//             }
//             env->ReleaseStringUTFChars(classNameStr, classNameChars);
//             success = 1;
//         }
//         env->DeleteLocalRef(classNameStr);
//     }
//     env->DeleteLocalRef(classClazz);
//     env->DeleteLocalRef(exceptionClazz);
//     if (! success) {
//         env->ExceptionClear();
//         snprintf(buf, bufLen, "%s", "<error getting class name>");
//     }
// }

// Each JNI function needs the types setup if we wish to use the types at all
struct TypeRegister
{
    JNIEnv* env;
    TypeRegister(JNIEnv* _env) : env(_env) {
        dmDefoldJNI::InitializeJNITypes(env);   DM_CHECK_JNI_ERROR();
        dmRenderJNI::InitializeJNITypes(env);   DM_CHECK_JNI_ERROR();
        dmRiveJNI::InitializeJNITypes(env);     DM_CHECK_JNI_ERROR();
    }
    ~TypeRegister() {
        dmRiveJNI::FinalizeJNITypes(env);   DM_CHECK_JNI_ERROR();
        dmRenderJNI::FinalizeJNITypes(env); DM_CHECK_JNI_ERROR();
        dmDefoldJNI::FinalizeJNITypes(env); DM_CHECK_JNI_ERROR();
    }
};

static jobject JNICALL Java_Rive_LoadFromBufferInternal(JNIEnv* env, jclass cls, jstring _path, jbyteArray array)
{
    DM_CHECK_JNI_ERROR();

    dmDefoldJNI::ScopedString j_path(env, _path);
    const char* path = j_path.m_String;

    const char* suffix = strrchr(path, '.');
    if (!suffix) {
        dmLogError("No suffix found in path: %s", path);
        return 0;
    } else {
        suffix++; // skip the '.'
    }

    jsize file_size = env->GetArrayLength(array);
    jbyte* file_data = env->GetByteArrayElements(array, 0);
    DM_CHECK_JNI_ERROR();

    TypeRegister register_t(env);

    jobject rive_file_obj = dmRiveJNI::LoadFileFromBuffer(env, cls, path, (const uint8_t*)file_data, (uint32_t)file_size);
    DM_CHECK_JNI_ERROR();

    if (dmLogGetLevel() == LOG_SEVERITY_DEBUG) // verbose mode
    {
        // Debug info
    }

    DM_CHECK_JNI_ERROR();
    return rive_file_obj;
}

static void JNICALL Java_Rive_Destroy(JNIEnv* env, jclass cls, jobject rive_file)
{
    DM_CHECK_JNI_ERROR();
    TypeRegister register_t(env);
    dmRiveJNI::DestroyFile(env, cls, rive_file);
    DM_CHECK_JNI_ERROR();
}

static void JNICALL Java_Rive_Update(JNIEnv* env, jclass cls, jobject rive_file, jfloat dt, jbyteArray texture_set_bytes)
{
    DM_CHECK_JNI_ERROR();

    jsize texture_set_size = 0;
    jbyte* texture_set_data = 0;

    if (texture_set_bytes != NULL)
    {
        texture_set_size = env->GetArrayLength(texture_set_bytes);
        texture_set_data = env->GetByteArrayElements(texture_set_bytes, 0);
        DM_CHECK_JNI_ERROR();
    }

    TypeRegister register_t(env);
    dmRiveJNI::Update(env, cls, rive_file, dt, (const uint8_t*) texture_set_data, (uint32_t) texture_set_size);
    DM_CHECK_JNI_ERROR();
}

// static JNIEXPORT jlong JNICALL Java_RiveFile_AddressOf(JNIEnv* env, jclass cls, jobject object)
// {
//     TypeRegister register_t(env);
//     return (jlong)(uintptr_t)object;
// }

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    dmLogDebug("JNI_OnLoad Rive ->\n");

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        printf("JNI_OnLoad GetEnv error\n");
        return JNI_ERR;
    }

    // Find your class. JNI_OnLoad is called from the correct class loader context for this to work.
    jclass c = env->FindClass("com/dynamo/bob/pipeline/Rive");
    dmLogDebug("JNI_OnLoad: c = %p\n", c);
    if (c == 0)
      return JNI_ERR;

    #define DM_JNI_FUNCTION(_NAME, _TYPES) {(char*) # _NAME, (char*) _TYPES, reinterpret_cast<void*>(Java_Rive_ ## _NAME)}

    // Register your class' native methods.
    static const JNINativeMethod methods[] = {
        DM_JNI_FUNCTION(LoadFromBufferInternal, "(Ljava/lang/String;[B)Lcom/dynamo/bob/pipeline/Rive$RiveFile;"),
        DM_JNI_FUNCTION(Destroy, "(Lcom/dynamo/bob/pipeline/Rive$RiveFile;)V"),
        DM_JNI_FUNCTION(Update, "(Lcom/dynamo/bob/pipeline/Rive$RiveFile;F[B)V"),
        //DM_JNI_FUNCTION(AddressOf, "(Ljava/lang/Object;)J"),
    };
    #undef DM_JNI_FUNCTION

    int rc = env->RegisterNatives(c, methods, sizeof(methods)/sizeof(JNINativeMethod));
    env->DeleteLocalRef(c);

    if (rc != JNI_OK) return rc;

    dmLogDebug("JNI_OnLoad return.\n");
    return JNI_VERSION_1_8;
}

