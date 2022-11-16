

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


#include <jni.h>

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

JNIEXPORT jobject JNICALL Java_RiveFile_LoadFromBufferInternal(JNIEnv* env, jclass cls, jstring _path, jbyteArray array)
{
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

    printf("LoadFromBufferInternal: %s suffix: %s bytes: %d\n", path, suffix, file_size);

    dmDefoldJNI::InitializeJNITypes(env);
    dmRenderJNI::InitializeJNITypes(env);
    dmRiveJNI::InitializeJNITypes(env);

    jobject rive_file_obj = dmRiveJNI::LoadFileFromBuffer(env, cls, path, (const uint8_t*)file_data, (uint32_t)file_size);

    if (dmLogGetLevel() == LOG_SEVERITY_DEBUG) // verbose mode
    {
        // Debug info
    }

    dmRiveJNI::FinalizeJNITypes(env);
    dmRenderJNI::FinalizeJNITypes(env);
    dmDefoldJNI::FinalizeJNITypes(env);

    printf("LoadFromBufferInternal: done!\n");

    return rive_file_obj;
}

static void JNICALL Java_RiveFile_Destroy(JNIEnv* env, jclass cls, jobject rive_file)
{
    dmRiveJNI::DestroyFile(env, cls, rive_file);
}

static void JNICALL Java_RiveFile_Update(JNIEnv* env, jclass cls, jobject rive_file, jfloat dt)
{
    dmRiveJNI::Update(env, cls, rive_file, dt);
}

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

    // Register your class' native methods.
    static const JNINativeMethod methods[] = {
        {(char*)"LoadFromBufferInternal", (char*)"(Ljava/lang/String;[B)Lcom/dynamo/bob/pipeline/Rive$RiveFile;", reinterpret_cast<void*>(Java_RiveFile_LoadFromBufferInternal)},
        {(char*)"Destroy", (char*)"(Lcom/dynamo/bob/pipeline/Rive$RiveFile;)V", reinterpret_cast<void*>(Java_RiveFile_Destroy)},
        {(char*)"Update", (char*)"(Lcom/dynamo/bob/pipeline/Rive$RiveFile;F)V", reinterpret_cast<void*>(Java_RiveFile_Update)},
        //{"AddressOf", "(Ljava/lang/Object;)I", reinterpret_cast<void*>(Java_Render_AddressOf)},
    };
    int rc = env->RegisterNatives(c, methods, sizeof(methods)/sizeof(JNINativeMethod));
    env->DeleteLocalRef(c);

    if (rc != JNI_OK) return rc;

    dmLogDebug("JNI_OnLoad return.\n");
    return JNI_VERSION_1_6;
}

