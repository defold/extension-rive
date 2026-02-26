

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
#include <rive/file_asset_loader.hpp>
// Due to an X11.h issue (Likely Ubuntu 16.04 issue) we include the Rive/C++17 includes first

#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/jobsystem.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/shared_library.h>
#include <dmsdk/dlib/static_assert.h>
#include <dmsdk/graphics/graphics.h>
#include <dmsdk/platform/window.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#include <pthread.h>
#endif

#include <defold/renderer.h>
#include <common/commands.h>

#include "jni/jni.h"

#include "crash.h"
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
    dmRiveCrash::ScopedSignalHandler signal_scope;

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
    dmRiveCrash::ScopedSignalHandler signal_scope;
    TypeRegister register_t(env);
    dmRiveJNI::DestroyFile(env, cls, rive_file);
    DM_CHECK_JNI_ERROR();
}

static void JNICALL Java_Rive_Update(JNIEnv* env, jclass cls, jobject rive_file, jfloat dt)
{
    DM_CHECK_JNI_ERROR();
    dmRiveCrash::ScopedSignalHandler signal_scope;

    TypeRegister register_t(env);
    dmRiveJNI::Update(env, cls, rive_file, dt);
    DM_CHECK_JNI_ERROR();
}

static void JNICALL Java_Rive_SetArtboard(JNIEnv* env, jclass cls, jobject rive_file, jstring _artboard)
{
    DM_CHECK_JNI_ERROR();
    dmRiveCrash::ScopedSignalHandler signal_scope;

    dmDefoldJNI::ScopedString j_artboard(env, _artboard);
    const char* name = j_artboard.m_String;

    TypeRegister register_t(env);
    dmRiveJNI::SetArtboard(env, cls, rive_file, name);
    DM_CHECK_JNI_ERROR();
}

static void JNICALL Java_Rive_SetStateMachine(JNIEnv* env, jclass cls, jobject rive_file, jstring _artboard)
{
    DM_CHECK_JNI_ERROR();
    dmRiveCrash::ScopedSignalHandler signal_scope;

    dmDefoldJNI::ScopedString j_artboard(env, _artboard);
    const char* name = j_artboard.m_String;

    TypeRegister register_t(env);
    dmRiveJNI::SetStateMachine(env, cls, rive_file, name);
    DM_CHECK_JNI_ERROR();
}

static void JNICALL Java_Rive_SetFitAlignment(JNIEnv* env, jclass cls, jobject rive_file, jint fit, jint alignment)
{
    DM_CHECK_JNI_ERROR();
    dmRiveCrash::ScopedSignalHandler signal_scope;

    TypeRegister register_t(env);
    dmRiveJNI::SetFitAlignment(env, cls, rive_file, fit, alignment);
    DM_CHECK_JNI_ERROR();
}

static void JNICALL Java_Rive_SetViewModel(JNIEnv* env, jclass cls, jobject rive_file, jstring _artboard)
{
    DM_CHECK_JNI_ERROR();
    dmRiveCrash::ScopedSignalHandler signal_scope;

    dmDefoldJNI::ScopedString j_artboard(env, _artboard);
    const char* name = j_artboard.m_String;

    TypeRegister register_t(env);
    dmRiveJNI::SetViewModel(env, cls, rive_file, name);
    DM_CHECK_JNI_ERROR();
}

static void CreateGraphicsContext();

static jobject JNICALL Java_Rive_GetTexture(JNIEnv* env, jclass cls, jobject rive_file)
{
    DM_CHECK_JNI_ERROR();
    dmRiveCrash::ScopedSignalHandler signal_scope;

    CreateGraphicsContext();

    TypeRegister register_t(env);
    jobject texture = dmRiveJNI::GetTexture(env, cls, rive_file);
    DM_CHECK_JNI_ERROR();
    return texture;
}

static jfloatArray JNICALL Java_Rive_GetFullscreenQuadVerticesInternal(JNIEnv* env, jclass cls)
{
    DM_CHECK_JNI_ERROR();
    dmRiveCrash::ScopedSignalHandler signal_scope;

    TypeRegister register_t(env);
    jfloatArray vertices = dmRenderJNI::CreateFullscreenQuadVertices(env);
    DM_CHECK_JNI_ERROR();
    return vertices;
}

// static JNIEXPORT jlong JNICALL Java_RiveFile_AddressOf(JNIEnv* env, jclass cls, jobject object)
// {
//     TypeRegister register_t(env);
//     return (jlong)(uintptr_t)object;
// }

dmRive::HRenderContext    g_RenderContext = 0;
HWindow                   g_Window = 0;
dmGraphics::HContext      g_GraphicsContext = 0;
HJobContext               g_JobContext = 0;

static dmGraphics::AdapterFamily      s_AdapterFamily = dmGraphics::ADAPTER_FAMILY_NONE;

static WindowsGraphicsApi GetWindowGraphicsApi(dmGraphics::AdapterFamily family)
{
    switch (family)
    {
        case dmGraphics::ADAPTER_FAMILY_OPENGL:     return WINDOW_GRAPHICS_API_OPENGL;
        case dmGraphics::ADAPTER_FAMILY_OPENGLES:   return WINDOW_GRAPHICS_API_OPENGLES;
        case dmGraphics::ADAPTER_FAMILY_DIRECTX:    return WINDOW_GRAPHICS_API_DIRECTX;
        case dmGraphics::ADAPTER_FAMILY_VULKAN:     return WINDOW_GRAPHICS_API_VULKAN;
        default:                        return WINDOW_GRAPHICS_API_VULKAN;
    }
}

static bool IsDebugLogEnabled()
{
#if defined(DM_RIVE_DEBUG_LOG)
    return true;
#else
    static int cached = -1;
    if (cached == -1)
    {
        const char* env = getenv("DM_RIVE_DEBUG_LOG");
        cached = (env && env[0] != '\0' && env[0] != '0') ? 1 : 0;
    }
    return cached == 1;
#endif
}

static void RiveDebugLog(const char* fmt, ...)
{
    if (!IsDebugLogEnabled())
    {
        return;
    }

    va_list args;
    va_start(args, fmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    dmLogInfo("%s", buffer);
    va_end(args);
}

#if defined(__APPLE__)
extern "C" void GraphicsAdapterVulkan();
#endif

static void InstallGraphicsAdapter()
{
#if defined(__APPLE__)
    GraphicsAdapterVulkan();
    s_AdapterFamily = dmGraphics::ADAPTER_FAMILY_VULKAN;
    dmGraphics::InstallAdapter(s_AdapterFamily);
#endif
}

static void CreateGraphicsContextInternal()
{
#if defined(__APPLE__)
    if (g_GraphicsContext != 0)
    {
        return;
    }

    RiveDebugLog("Rive: creating window for graphics context");
    g_Window = WindowNew();
    if (!g_Window)
    {
        dmLogError("Rive: failed to create window");
        return;
    }
    RiveDebugLog("Rive: window created");

    JobSystemCreateParams job_params = { 0 };
    g_JobContext = JobSystemCreate(&job_params);

    WindowCreateParams window_params;
    WindowCreateParamsInitialize(&window_params);
    window_params.m_Width = 512;
    window_params.m_Height = 512;
    window_params.m_Title = "Rive Plugin";
    window_params.m_GraphicsApi = GetWindowGraphicsApi(s_AdapterFamily);
    window_params.m_Hidden = 1;

    RiveDebugLog("Rive: opening window");
    WindowOpen(g_Window, &window_params);
    RiveDebugLog("Rive: window opened");
    WindowPollEvents(g_Window);

    dmGraphics::ContextParams graphics_context_params;
    graphics_context_params.m_DefaultTextureMinFilter = dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
    graphics_context_params.m_DefaultTextureMagFilter = dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
    graphics_context_params.m_VerifyGraphicsCalls = 1;
    graphics_context_params.m_UseValidationLayers = 1;
    graphics_context_params.m_Window = g_Window;
    graphics_context_params.m_Width = 512;
    graphics_context_params.m_Height = 512;
    graphics_context_params.m_JobContext = g_JobContext;

    RiveDebugLog("Rive: creating graphics context");
    g_GraphicsContext = dmGraphics::NewContext(graphics_context_params);
    if (!g_GraphicsContext)
    {
        dmLogError("Rive: failed to create graphics context");
    }
    else
    {
        RiveDebugLog("Rive: graphics context created");
    }
#endif
}

static void CreateGraphicsContext()
{
#if defined(__APPLE__)
    if (g_GraphicsContext != 0)
    {
        return;
    }

    if (!pthread_main_np())
    {
        RiveDebugLog("Rive: dispatching graphics context creation to main thread");
        dispatch_sync(dispatch_get_main_queue(), ^{
            CreateGraphicsContextInternal();
        });
    }
    else
    {
        CreateGraphicsContextInternal();
    }
#else
    CreateGraphicsContextInternal();
#endif
}

static void PluginRiveInitialize()
{
    InstallGraphicsAdapter();
    g_RenderContext = dmRive::NewRenderContext();
    assert(g_RenderContext != 0);

    dmRiveCommands::InitParams cmd_params;
    cmd_params.m_UseThreads = true; // TODO: Use define and/or config flag
    cmd_params.m_RenderContext = g_RenderContext;
    cmd_params.m_Factory = dmRive::GetRiveFactory(g_RenderContext);
    dmRiveCommands::Initialize(&cmd_params);
}

static void PluginRiveFinalize()
{
    if (g_RenderContext)
    {
        dmRiveCommands::Finalize();
        dmRive::DeleteRenderContext(g_RenderContext);
        g_RenderContext = 0;
    }

#if defined(__APPLE__)
    if (g_GraphicsContext)
    {
        dmGraphics::CloseWindow(g_GraphicsContext);
        dmGraphics::DeleteContext(g_GraphicsContext);
        dmGraphics::Finalize();
        g_GraphicsContext = 0;
    }

    if (g_Window)
    {
        g_Window = 0;
    }

    if (g_JobContext)
    {
        JobSystemDestroy(g_JobContext);
        g_JobContext = 0;
    }
#endif
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

    #define DM_JNI_FUNCTION(_NAME, _TYPES) {(char*) # _NAME, (char*) _TYPES, reinterpret_cast<void*>(Java_Rive_ ## _NAME)}

    // Register your class' native methods.
    static const JNINativeMethod methods[] = {
        DM_JNI_FUNCTION(LoadFromBufferInternal, "(Ljava/lang/String;[B)Lcom/dynamo/bob/pipeline/Rive$RiveFile;"),
        DM_JNI_FUNCTION(Destroy, "(Lcom/dynamo/bob/pipeline/Rive$RiveFile;)V"),
        DM_JNI_FUNCTION(Update, "(Lcom/dynamo/bob/pipeline/Rive$RiveFile;F)V"),
        DM_JNI_FUNCTION(SetArtboard, "(Lcom/dynamo/bob/pipeline/Rive$RiveFile;Ljava/lang/String;)V"),
        DM_JNI_FUNCTION(SetStateMachine, "(Lcom/dynamo/bob/pipeline/Rive$RiveFile;Ljava/lang/String;)V"),
        DM_JNI_FUNCTION(SetFitAlignment, "(Lcom/dynamo/bob/pipeline/Rive$RiveFile;II)V"),
        DM_JNI_FUNCTION(SetViewModel, "(Lcom/dynamo/bob/pipeline/Rive$RiveFile;Ljava/lang/String;)V"),
        DM_JNI_FUNCTION(GetTexture, "(Lcom/dynamo/bob/pipeline/Rive$RiveFile;)Lcom/dynamo/bob/pipeline/Rive$Texture;"),
        DM_JNI_FUNCTION(GetFullscreenQuadVerticesInternal, "()[F")
        //DM_JNI_FUNCTION(AddressOf, "(Ljava/lang/Object;)J"),
    };
    #undef DM_JNI_FUNCTION

    int rc = env->RegisterNatives(c, methods, sizeof(methods)/sizeof(JNINativeMethod));
    env->DeleteLocalRef(c);

    if (rc != JNI_OK) return rc;

    PluginRiveInitialize();

    dmLogDebug("JNI_OnLoad return.\n");
    return JNI_VERSION_1_8;
}

JNIEXPORT void JNI_OnUnload(JavaVM* vm, void* reserved)
{
    (void)vm;
    (void)reserved;
    dmLogDebug("JNI_OnUnload Rive ->\n");

    PluginRiveFinalize();

    dmLogDebug("JNI_OnUnload return.\n");
}
