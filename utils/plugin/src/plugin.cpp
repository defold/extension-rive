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
#include <dmsdk/dlib/mutex.h>
#include <dmsdk/dlib/shared_library.h>
#include <dmsdk/dlib/static_assert.h>
#include <dmsdk/graphics/graphics.h>
#if defined(DM_GRAPHICS_USE_VULKAN)
#include <dmsdk/graphics/graphics_vulkan.h>
#endif
#include <dmsdk/platform/window.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#include <pthread.h>
#endif
#if defined(_WIN32)
#include <dmsdk/dlib/safe_windows.h>
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

static bool CreateGraphicsContext();
static bool PluginRiveInitialize();
extern dmRive::HRenderContext g_RenderContext;
extern dmGraphics::HContext g_GraphicsContext;
static dmMutex::HMutex g_JNINativeCallMutex = 0;

static bool WaitForCommandServer()
{
    if (g_RenderContext == 0)
    {
        return true;
    }

    if (dmRiveCommands::ProcessMessages() != dmRiveCommands::RESULT_OK)
    {
        dmLogError("Rive: failed to process command server messages");
        return false;
    }
    return true;
}

static jobject JNICALL Java_Rive_LoadFromBufferInternal(JNIEnv* env, jclass cls, jstring _path, jbyteArray array)
{
    // The editor invokes plugin JNI methods from a worker pool.
    // Serialize all native calls to keep JNI class refs + graphics state thread-safe.
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_JNINativeCallMutex);
    DM_CHECK_JNI_ERROR();
    dmRiveCrash::ScopedSignalHandler signal_scope;

    if (!CreateGraphicsContext())
    {
        dmLogError("Rive: failed to create graphics context");
        return 0;
    }

    if (!PluginRiveInitialize() || g_RenderContext == 0)
    {
        dmLogError("Rive: render context was not initialized");
        return 0;
    }

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

    if (!WaitForCommandServer())
    {
        return 0;
    }

    if (dmLogGetLevel() == LOG_SEVERITY_DEBUG) // verbose mode
    {
        // Debug info
    }

    DM_CHECK_JNI_ERROR();
    return rive_file_obj;
}

static void JNICALL Java_Rive_Destroy(JNIEnv* env, jclass cls, jobject rive_file)
{
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_JNINativeCallMutex);
    DM_CHECK_JNI_ERROR();
    dmRiveCrash::ScopedSignalHandler signal_scope;
    TypeRegister register_t(env);
    dmRiveJNI::DestroyFile(env, cls, rive_file);
    DM_CHECK_JNI_ERROR();
    WaitForCommandServer();
}

static void JNICALL Java_Rive_Update(JNIEnv* env, jclass cls, jobject rive_file, jfloat dt)
{
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_JNINativeCallMutex);
    DM_CHECK_JNI_ERROR();
    dmRiveCrash::ScopedSignalHandler signal_scope;

    TypeRegister register_t(env);
    dmRiveJNI::Update(env, cls, rive_file, dt);
    DM_CHECK_JNI_ERROR();
    WaitForCommandServer();
}

static void JNICALL Java_Rive_SetArtboard(JNIEnv* env, jclass cls, jobject rive_file, jstring _artboard)
{
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_JNINativeCallMutex);
    DM_CHECK_JNI_ERROR();
    dmRiveCrash::ScopedSignalHandler signal_scope;

    dmDefoldJNI::ScopedString j_artboard(env, _artboard);
    const char* name = j_artboard.m_String;

    TypeRegister register_t(env);
    dmRiveJNI::SetArtboard(env, cls, rive_file, name);
    DM_CHECK_JNI_ERROR();
    WaitForCommandServer();
}

static void JNICALL Java_Rive_SetStateMachine(JNIEnv* env, jclass cls, jobject rive_file, jstring _artboard)
{
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_JNINativeCallMutex);
    DM_CHECK_JNI_ERROR();
    dmRiveCrash::ScopedSignalHandler signal_scope;

    dmDefoldJNI::ScopedString j_artboard(env, _artboard);
    const char* name = j_artboard.m_String;

    TypeRegister register_t(env);
    dmRiveJNI::SetStateMachine(env, cls, rive_file, name);
    DM_CHECK_JNI_ERROR();
    WaitForCommandServer();
}

static void JNICALL Java_Rive_SetFitAlignment(JNIEnv* env, jclass cls, jobject rive_file, jint fit, jint alignment)
{
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_JNINativeCallMutex);
    DM_CHECK_JNI_ERROR();
    dmRiveCrash::ScopedSignalHandler signal_scope;

    TypeRegister register_t(env);
    dmRiveJNI::SetFitAlignment(env, cls, rive_file, fit, alignment);
    DM_CHECK_JNI_ERROR();
    WaitForCommandServer();
}

static void JNICALL Java_Rive_SetViewModel(JNIEnv* env, jclass cls, jobject rive_file, jstring _artboard)
{
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_JNINativeCallMutex);
    DM_CHECK_JNI_ERROR();
    dmRiveCrash::ScopedSignalHandler signal_scope;

    dmDefoldJNI::ScopedString j_artboard(env, _artboard);
    const char* name = j_artboard.m_String;

    TypeRegister register_t(env);
    dmRiveJNI::SetViewModel(env, cls, rive_file, name);
    DM_CHECK_JNI_ERROR();
    WaitForCommandServer();
}

static jobject JNICALL Java_Rive_GetTexture(JNIEnv* env, jclass cls, jobject rive_file)
{
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_JNINativeCallMutex);
    DM_CHECK_JNI_ERROR();
    dmRiveCrash::ScopedSignalHandler signal_scope;

    if (!CreateGraphicsContext())
    {
        dmLogError("Rive: failed to create graphics context");
        return 0;
    }

    if (!PluginRiveInitialize() || g_RenderContext == 0)
    {
        dmLogError("Rive: render context was not initialized");
        return 0;
    }

    TypeRegister register_t(env);
    jobject texture = dmRiveJNI::GetTexture(env, cls, rive_file);
    DM_CHECK_JNI_ERROR();
    if (!WaitForCommandServer())
    {
        return 0;
    }
    return texture;
}

static jfloatArray JNICALL Java_Rive_GetFullscreenQuadVerticesInternal(JNIEnv* env, jclass cls)
{
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_JNINativeCallMutex);
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
dmMutex::HMutex           g_RenderMutex = 0;
HWindow                   g_Window = 0;
dmGraphics::HContext      g_GraphicsContext = 0;
HJobContext               g_JobContext = 0;
static bool               s_AdapterInstallAttempted = false;
static bool               s_AdapterInstallSuccess = false;

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

#if defined(DM_GRAPHICS_USE_VULKAN)
extern "C" void GraphicsAdapterVulkan();
#else
extern "C" void GraphicsAdapterOpenGL();
#endif

static bool InstallGraphicsAdapter()
{
    if (s_AdapterInstallAttempted)
    {
        return s_AdapterInstallSuccess;
    }
    s_AdapterInstallAttempted = true;

#if defined(DM_GRAPHICS_USE_VULKAN)
    GraphicsAdapterVulkan(); // register the adapter
#else
    GraphicsAdapterOpenGL(); // register the adapter
#endif
    s_AdapterInstallSuccess = dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_NONE);
    return s_AdapterInstallSuccess;
}

static bool CreateGraphicsContextInternal()
{
    if (g_GraphicsContext != 0)
    {
        return true;
    }

    if (!InstallGraphicsAdapter())
    {
        dmLogError("Rive: failed to install graphics adapter");
        return false;
    }

    RiveDebugLog("Rive: creating window for graphics context");
    g_Window = WindowNew();
    if (!g_Window)
    {
        dmLogError("Rive: failed to create window");
        return false;
    }
    RiveDebugLog("Rive: window created");

    JobSystemCreateParams job_params = { 0 };
    job_params.m_ThreadNamePrefix = "RivePlugin";
    // Keep plugin-side rendering single-threaded in the editor to avoid
    // cross-thread context/assert issues.
    job_params.m_ThreadCount = 1;
    g_JobContext = JobSystemCreate(&job_params);
    if (!g_JobContext)
    {
        dmLogError("Rive: failed to create job context");
        return false;
    }

    WindowCreateParams window_params;
    WindowCreateParamsInitialize(&window_params);
    window_params.m_Width = 512;
    window_params.m_Height = 512;
    window_params.m_Title = "Rive Plugin";
#if defined(DM_GRAPHICS_USE_VULKAN)
    window_params.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
#else
    window_params.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
#endif
#if defined(__linux__)
    // Rive's OpenGL renderer requires at least OpenGL 4.2.
    window_params.m_OpenGLVersionHint = 42;
    window_params.m_OpenGLUseCoreProfileHint = 1;
#endif
    window_params.m_Hidden = 1;

    RiveDebugLog("Rive: opening window");
    WindowResult open_result = WindowOpen(g_Window, &window_params);
    if (open_result != WINDOW_RESULT_OK)
    {
        dmLogError("Rive: failed to open window (%d)", (int)open_result);
        WindowDelete(g_Window);
        g_Window = 0;
        return false;
    }
    RiveDebugLog("Rive: window opened");
    WindowPollEvents(g_Window);

    dmGraphics::ContextParams graphics_context_params = {};
    graphics_context_params.m_DefaultTextureMinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
    graphics_context_params.m_DefaultTextureMagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
    graphics_context_params.m_VerifyGraphicsCalls = 0;
    graphics_context_params.m_UseValidationLayers = 0;
    graphics_context_params.m_Window = g_Window;
    graphics_context_params.m_Width = 512;
    graphics_context_params.m_Height = 512;
    graphics_context_params.m_JobContext = g_JobContext;

    RiveDebugLog("Rive: creating graphics context");
    g_GraphicsContext = dmGraphics::NewContext(graphics_context_params);
    if (!g_GraphicsContext)
    {
        dmLogError("Rive: failed to create graphics context");
        return false;
    }
    RiveDebugLog("Rive: graphics context created");
    return true;
}

#if defined(__APPLE__)
static void CreateGraphicsContextOnMainThread(void* ctx)
{
    bool* created = (bool*)ctx;
    *created = CreateGraphicsContextInternal();
}
#endif

static bool CreateGraphicsContext()
{
#if defined(__APPLE__)
    if (g_GraphicsContext != 0)
    {
        return true;
    }

    bool created = false;
    if (!pthread_main_np())
    {
        RiveDebugLog("Rive: dispatching graphics context creation to main thread");
        dispatch_sync_f(dispatch_get_main_queue(), &created, CreateGraphicsContextOnMainThread);
    }
    else
    {
        created = CreateGraphicsContextInternal();
    }
    return created;
#else
    return CreateGraphicsContextInternal();
#endif
}

static bool PluginRiveInitialize()
{
    if (g_RenderContext != 0)
    {
        return true;
    }

    if (!InstallGraphicsAdapter())
    {
        dmLogError("Rive: failed to install graphics adapter");
        return false;
    }

    g_RenderContext = dmRive::NewRenderContext();
    if (g_RenderContext == 0)
    {
        dmLogError("Rive: failed to create render context");
        return false;
    }
    g_RenderMutex = dmMutex::New();
    if (g_RenderMutex == 0)
    {
        dmLogError("Rive: failed to create render mutex");
        dmRive::DeleteRenderContext(g_RenderContext);
        g_RenderContext = 0;
        return false;
    }
    dmRive::SetRenderMutex(g_RenderContext, g_RenderMutex);

    dmRiveCommands::InitParams cmd_params;
    cmd_params.m_UseThreads = false;
    cmd_params.m_RenderContext = g_RenderContext;
    cmd_params.m_Factory = dmRive::GetRiveFactory(g_RenderContext);
    cmd_params.m_Mutex = g_RenderMutex;
    if (cmd_params.m_Factory == 0)
    {
        dmLogError("Rive: failed to get Rive factory");
        dmRive::SetRenderMutex(g_RenderContext, 0);
        dmMutex::Delete(g_RenderMutex);
        g_RenderMutex = 0;
        dmRive::DeleteRenderContext(g_RenderContext);
        g_RenderContext = 0;
        return false;
    }
    dmRiveCommands::Initialize(&cmd_params);
    return true;
}

static void PluginRiveFinalize()
{
#if defined(DM_GRAPHICS_USE_VULKAN)
    if (g_GraphicsContext)
    {
        VkDevice device = dmGraphics::VulkanGetDevice(g_GraphicsContext);
        if (device != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(device);
        }
    }
#endif

    if (g_RenderContext)
    {
        dmRiveCommands::Finalize();
        dmRive::SetRenderMutex(g_RenderContext, 0);
        dmRive::DeleteRenderContext(g_RenderContext);
        g_RenderContext = 0;
    }
    if (g_RenderMutex)
    {
        dmMutex::Delete(g_RenderMutex);
        g_RenderMutex = 0;
    }

    if (g_GraphicsContext)
    {
        dmGraphics::CloseWindow(g_GraphicsContext);
        dmGraphics::DeleteContext(g_GraphicsContext);
        dmGraphics::Finalize();
        g_GraphicsContext = 0;
    }
    g_Window = 0;

    if (g_JobContext)
    {
        JobSystemDestroy(g_JobContext);
        g_JobContext = 0;
    }
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    dmLogDebug("JNI_OnLoad Rive ->\n");
    if (g_JNINativeCallMutex == 0)
    {
        g_JNINativeCallMutex = dmMutex::New();
    }

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

    dmLogDebug("JNI_OnLoad return.\n");
    return JNI_VERSION_1_8;
}

JNIEXPORT void JNI_OnUnload(JavaVM* vm, void* reserved)
{
    (void)vm;
    (void)reserved;
    dmLogDebug("JNI_OnUnload Rive ->\n");

    PluginRiveFinalize();
    if (g_JNINativeCallMutex)
    {
        dmMutex::Delete(g_JNINativeCallMutex);
        g_JNINativeCallMutex = 0;
    }

    dmLogDebug("JNI_OnUnload return.\n");
}
