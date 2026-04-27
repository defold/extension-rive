// Copyright 2020-2025 The Defold Foundation
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

// Creating a small app test for initializing an running a small graphics app

#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/jobsystem.h>
#include <dmsdk/dlib/mutex.h>
#include <dmsdk/dlib/time.h>
#include <dmsdk/graphics/graphics.h>
#include <dmsdk/platform/window.h>
#include <dmsdk/render/render.h>

#if defined(DM_GRAPHICS_USE_VULKAN)
#include <dmsdk/graphics/graphics_vulkan.h>
#endif

#include <dmsdk/dlib/log.h> // LogParams

#include <defold/rive.h>
#include <rive/shapes/paint/color.hpp>
#include <rive/factory.hpp>
#include <utils/factory_utils.hpp>
#include "common/file.h"
#include "texture.h"
#include <common/commands.h>

namespace dmGraphics
{
    ShaderDesc* CreateRiveModelBlitShaderDesc();
}

enum UpdateResult
{
    RESULT_OK       =  0,
    RESULT_REBOOT   =  1,
    RESULT_EXIT     = -1,
};

typedef void* (*EngineCreateFn)(int argc, char** argv);
typedef void (*EngineDestroyFn)(void* engine);
typedef UpdateResult (*EngineUpdateFn)(void* engine);
typedef void (*EngineGetResultFn)(void* engine, int* run_action, int* exit_code, int* argc, char*** argv);


static const char*                  s_RiveFilePath = 0;
static bool                         s_ScreenshotCaptured = false;
static uint32_t                     s_ScreenshotDelayFrames = 5;
static dmGraphics::AdapterFamily    s_AdapterFamily = dmGraphics::ADAPTER_FAMILY_NONE;
static bool                         s_HeadlessMode = false;

class ViewerNoOpRenderShader : public rive::RenderShader
{
};

class ViewerNoOpRenderPaint : public rive::RenderPaint
{
public:
    void style(rive::RenderPaintStyle value) override { (void)value; }
    void color(rive::ColorInt value) override { (void)value; }
    void thickness(float value) override { (void)value; }
    void join(rive::StrokeJoin value) override { (void)value; }
    void cap(rive::StrokeCap value) override { (void)value; }
    void blendMode(rive::BlendMode value) override { (void)value; }
    void shader(rive::rcp<rive::RenderShader> value) override { (void)value; }
    void invalidateStroke() override {}
};

class ViewerNoOpRenderPath : public rive::RenderPath
{
public:
    void rewind() override {}
    void fillRule(rive::FillRule value) override { (void)value; }
    void addRenderPath(const rive::RenderPath* path, const rive::Mat2D& transform) override
    {
        (void)path;
        (void)transform;
    }
    void addRawPath(const rive::RawPath& path) override { (void)path; }
    void moveTo(float x, float y) override
    {
        (void)x;
        (void)y;
    }
    void lineTo(float x, float y) override
    {
        (void)x;
        (void)y;
    }
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y) override
    {
        (void)ox;
        (void)oy;
        (void)ix;
        (void)iy;
        (void)x;
        (void)y;
    }
    void close() override {}
};

class ViewerNoOpFactory : public rive::Factory
{
public:
    rive::rcp<rive::RenderBuffer> makeRenderBuffer(rive::RenderBufferType type,
                                                   rive::RenderBufferFlags flags,
                                                   size_t sizeInBytes) override
    {
        return rive::make_rcp<rive::DataRenderBuffer>(type, flags, sizeInBytes);
    }

    rive::rcp<rive::RenderShader> makeLinearGradient(float sx,
                                                     float sy,
                                                     float ex,
                                                     float ey,
                                                     const rive::ColorInt colors[],
                                                     const float stops[],
                                                     size_t count) override
    {
        (void)sx;
        (void)sy;
        (void)ex;
        (void)ey;
        (void)colors;
        (void)stops;
        (void)count;
        return rive::make_rcp<ViewerNoOpRenderShader>();
    }

    rive::rcp<rive::RenderShader> makeRadialGradient(float cx,
                                                     float cy,
                                                     float radius,
                                                     const rive::ColorInt colors[],
                                                     const float stops[],
                                                     size_t count) override
    {
        (void)cx;
        (void)cy;
        (void)radius;
        (void)colors;
        (void)stops;
        (void)count;
        return rive::make_rcp<ViewerNoOpRenderShader>();
    }

    rive::rcp<rive::RenderPath> makeRenderPath(rive::RawPath& path, rive::FillRule fill_rule) override
    {
        (void)path;
        (void)fill_rule;
        return rive::make_rcp<ViewerNoOpRenderPath>();
    }

    rive::rcp<rive::RenderPath> makeEmptyRenderPath() override
    {
        return rive::make_rcp<ViewerNoOpRenderPath>();
    }

    rive::rcp<rive::RenderPaint> makeRenderPaint() override
    {
        return rive::make_rcp<ViewerNoOpRenderPaint>();
    }

    rive::rcp<rive::RenderImage> decodeImage(rive::Span<const uint8_t> bytes) override
    {
        (void)bytes;
        return nullptr;
    }
};

class ViewerRenderImageListener : public rive::CommandQueue::RenderImageListener
{
public:
    void onRenderImageDecoded(const rive::RenderImageHandle image, uint64_t request_id) override
    {
        dmLogInfo("RenderImage decoded: handle=%llu request_id=%llu",
                  (unsigned long long)image,
                  (unsigned long long)request_id);
    }

    void onRenderImageError(const rive::RenderImageHandle image,
                            uint64_t request_id,
                            std::string error) override
    {
        dmLogError("RenderImage error: handle=%llu request_id=%llu error=%s",
                   (unsigned long long)image,
                   (unsigned long long)request_id,
                   error.c_str());
    }

    void onRenderImageDeleted(const rive::RenderImageHandle image, uint64_t request_id) override
    {
        dmLogInfo("RenderImage deleted: handle=%llu request_id=%llu",
                  (unsigned long long)image,
                  (unsigned long long)request_id);
    }
};

static ViewerRenderImageListener s_RenderImageListener;

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

static bool WriteTgaFile(const char* path, dmRive::TexturePixels* pixels)
{
    if (!path || !pixels || !pixels->m_Data || pixels->m_Width == 0 || pixels->m_Height == 0)
    {
        return false;
    }

    if (pixels->m_Format == dmGraphics::TEXTURE_FORMAT_RGBA)
    {
        uint32_t pixel_count = (uint32_t)pixels->m_Width * (uint32_t)pixels->m_Height;
        uint8_t* data = pixels->m_Data;
        for (uint32_t i = 0; i < pixel_count; ++i)
        {
            uint8_t tmp = data[0];
            data[0] = data[2];
            data[2] = tmp;
            data += 4;
        }
        pixels->m_Format = dmGraphics::TEXTURE_FORMAT_BGRA8U;
    }

    if (pixels->m_Format != dmGraphics::TEXTURE_FORMAT_BGRA8U)
    {
        return false;
    }

    FILE* f = fopen(path, "wb");
    if (!f)
    {
        return false;
    }

    // TGA header: image type, width/height, 32 bpp, 8-bit alpha.
    uint8_t header[18];
    memset(header, 0, sizeof(header));
    header[2] = 2; // Uncompressed true-color image.
    header[12] = (uint8_t)(pixels->m_Width & 0xFF); // Width (low byte).
    header[13] = (uint8_t)((pixels->m_Width >> 8) & 0xFF); // Width (high byte).
    header[14] = (uint8_t)(pixels->m_Height & 0xFF); // Height (low byte).
    header[15] = (uint8_t)((pixels->m_Height >> 8) & 0xFF); // Height (high byte).
    header[16] = 32; // Bits per pixel.
    // OpenGL texture readback is bottom-left ordered, while the Vulkan
    // backbuffer path is already top-left ordered in the saved screenshot.
    header[17] = 8;
    if (s_AdapterFamily != dmGraphics::ADAPTER_FAMILY_OPENGL &&
        s_AdapterFamily != dmGraphics::ADAPTER_FAMILY_OPENGLES)
    {
        header[17] |= 0x20; // Top-left origin.
    }

    fwrite(header, 1, sizeof(header), f);
    fwrite(pixels->m_Data, 1, pixels->m_DataSize, f);
    fclose(f);
    return true;
}

static bool ReadFile(const char* path, std::vector<uint8_t>& out)
{
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "Failed to open '%s'\n", path);
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return false;
    }
    long size = ftell(f);
    if (size < 0)
    {
        fclose(f);
        return false;
    }
    rewind(f);

    out.resize(size);
    if (size > 0)
    {
        size_t read = fread(out.data(), 1, size, f);
        if (read != (size_t)size)
        {
            fclose(f);
            return false;
        }
    }

    fclose(f);
    return true;
}

static uint32_t BoundsDimension(float min_value, float max_value, uint32_t fallback)
{
    float size = max_value - min_value;
    if (size <= 0.0f)
    {
        return fallback;
    }

    uint32_t dim = (uint32_t)ceilf(size);
    return dim > 0 ? dim : fallback;
}

static bool IsBoundsValid(const rive::AABB& bounds)
{
    return !bounds.isEmptyOrNaN();
}

static bool CreateSolidBluePixels(uint32_t width, uint32_t height, dmRive::TexturePixels* pixels)
{
    if (!pixels || width == 0 || height == 0)
    {
        return false;
    }

    const uint32_t data_size = width * height * 4;
    uint8_t* data = (uint8_t*)malloc(data_size);
    if (!data)
    {
        return false;
    }

    for (uint32_t i = 0; i < width * height; ++i)
    {
        uint8_t* pixel = data + i * 4;
        pixel[0] = 139; // B
        pixel[1] = 0;   // G
        pixel[2] = 0;   // R
        pixel[3] = 255; // A
    }

    pixels->m_Data = data;
    pixels->m_DataSize = data_size;
    pixels->m_Width = (uint16_t)width;
    pixels->m_Height = (uint16_t)height;
    pixels->m_Format = dmGraphics::TEXTURE_FORMAT_BGRA8U;
    return true;
}

struct AppCtx
{
    int m_Created;
    int m_Destroyed;
};

struct EngineCtx
{
    int m_WasCreated;
    int m_WasRun;
    int m_WasDestroyed;
    int                            m_WasResultCalled;
    int                            m_Running;
    bool                           m_WindowClosed;
    bool                           m_Headless;
    bool                           m_GraphicsBackendAvailable;

    HJobContext                    m_JobContext;
    HWindow                        m_Window;
    dmGraphics::HContext           m_GraphicsContext;

    dmGraphics::HVertexBuffer      m_BlitToBackbufferVertexBuffer;
    dmGraphics::HVertexDeclaration m_VertexDeclaration;
    dmGraphics::HProgram           m_BlitProgram;
    dmGraphics::HTexture           m_Texture;
    dmGraphics::HUniformLocation   m_SamplerLocation;

    // Rive related
    dmRive::HRenderContext          m_RenderContext;
    dmMutex::HMutex                 m_RenderMutex;
    dmRive::RiveFile*               m_FileMeta;
    rive::Factory*                  m_Factory;

    rive::FileHandle                m_File;
    rive::ArtboardHandle            m_Artboard;
    rive::StateMachineHandle        m_StateMachine;
    rive::ViewModelInstanceHandle   m_ViewModelInstance;
    rive::DrawKey                   m_DrawKey;
};

static void ResolveScreenshotSize(EngineCtx* engine, uint32_t* out_width, uint32_t* out_height)
{
    uint32_t width = 512;
    uint32_t height = 512;

    if (engine->m_GraphicsContext)
    {
        width = dmGraphics::GetWindowWidth(engine->m_GraphicsContext);
        height = dmGraphics::GetWindowHeight(engine->m_GraphicsContext);
    }

    if (!engine->m_FileMeta)
    {
        *out_width = width;
        *out_height = height;
        return;
    }

    rive::AABB bounds = engine->m_FileMeta->m_Bounds;
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    if (queue && engine->m_Artboard != RIVE_NULL_HANDLE)
    {
        rive::AABB fresh_bounds;
        if (dmRiveCommands::GetBounds(engine->m_Artboard, &fresh_bounds) && IsBoundsValid(fresh_bounds))
        {
            bounds = fresh_bounds;
        }
    }

    if (IsBoundsValid(bounds))
    {
        width = BoundsDimension(bounds.minX, bounds.maxX, width);
        height = BoundsDimension(bounds.minY, bounds.maxY, height);
    }

    *out_width = width;
    *out_height = height;
}

static bool CaptureFallbackScreenshot(EngineCtx* engine)
{
    uint32_t width = 0;
    uint32_t height = 0;
    ResolveScreenshotSize(engine, &width, &height);

    dmRive::TexturePixels pixels = {};
    if (!CreateSolidBluePixels(width, height, &pixels))
    {
        return false;
    }

    const char* screenshot_path = "./screenshot.tga";
    bool ok = WriteTgaFile(screenshot_path, &pixels);
    if (ok)
    {
        printf("Wrote fallback screenshot %s (%ux%u)\n", screenshot_path, width, height);
    }
    else
    {
        printf("Failed to write fallback screenshot to %s\n", screenshot_path);
    }
    dmRive::FreePixels(&pixels);
    return ok;
}

struct RunLoopParams
{
    int     m_Argc;
    char**  m_Argv;

    void*   m_AppCtx;
    void    (*m_AppCreate)(void* ctx);
    void    (*m_AppDestroy)(void* ctx);

    EngineCreateFn      m_EngineCreate;
    EngineDestroyFn     m_EngineDestroy;
    EngineUpdateFn      m_EngineUpdate;
    EngineGetResultFn   m_EngineGetResult;
};

static int RunLoop(const RunLoopParams* params)
{
    if (params->m_AppCreate)
        params->m_AppCreate(params->m_AppCtx);

    int argc = params->m_Argc;
    char** argv = params->m_Argv;
    int exit_code = 0;
    void* engine = 0;
    UpdateResult result = RESULT_OK;
    while (RESULT_OK == result)
    {
        if (engine == 0)
        {
            engine = params->m_EngineCreate(argc, argv);
            if (!engine)
            {
                exit_code = 1;
                break;
            }
        }

        result = params->m_EngineUpdate(engine);

        if (RESULT_OK != result)
        {
            int run_action = 0;
            params->m_EngineGetResult(engine, &run_action, &exit_code, &argc, &argv);

            params->m_EngineDestroy(engine);
            engine = 0;

            if (RESULT_REBOOT == result)
            {
                // allows us to reboot
                result = RESULT_OK;
            }
        }
    }

    if (params->m_AppDestroy)
        params->m_AppDestroy(params->m_AppCtx);

    return exit_code;
}

static void AppCreate(void* _ctx)
{
    //dmLog::LogParams params;
    int params = 0;
    dmLog::LogInitialize((const dmLog::LogParams*)&params);

    AppCtx* ctx = (AppCtx*)_ctx;
    ctx->m_Created++;
}

static void AppDestroy(void* _ctx)
{
    AppCtx* ctx = (AppCtx*)_ctx;
    ctx->m_Destroyed++;

    dmLog::LogFinalize();
}

static void DrawFullscreenQuad(EngineCtx* engine, dmGraphics::HTexture texture)
{
    if (engine->m_BlitProgram == 0 || engine->m_BlitProgram == dmGraphics::INVALID_PROGRAM_HANDLE || texture == 0)
    {
        return;
    }

    dmGraphics::HContext context = engine->m_GraphicsContext;
    uint32_t window_width = dmGraphics::GetWindowWidth(context);
    uint32_t window_height = dmGraphics::GetWindowHeight(context);
    dmGraphics::SetViewport(context, 0, 0, window_width, window_height);
    dmGraphics::EnableProgram(context, engine->m_BlitProgram);
    dmGraphics::EnableVertexBuffer(context, engine->m_BlitToBackbufferVertexBuffer, 0);
    dmGraphics::EnableVertexDeclaration(context, engine->m_VertexDeclaration, 0, 0, engine->m_BlitProgram);

    dmGraphics::EnableTexture(context, 0, 0, texture);
    if (engine->m_SamplerLocation != dmGraphics::INVALID_UNIFORM_LOCATION)
    {
        dmGraphics::SetSampler(context, engine->m_SamplerLocation, 0);
    }

    dmGraphics::Draw(context, dmGraphics::PRIMITIVE_TRIANGLES, 0, 6, 1);

    dmGraphics::DisableTexture(context, 0, texture);
    dmGraphics::DisableVertexBuffer(context, engine->m_BlitToBackbufferVertexBuffer);
    dmGraphics::DisableVertexDeclaration(context, engine->m_VertexDeclaration);
    dmGraphics::DisableProgram(context);
}

static int OnWindowClose(void* user_data)
{
    EngineCtx* engine = (EngineCtx*)user_data;
    engine->m_WindowClosed = 1;
    return 1;
}

static void* EngineCreate(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    EngineCtx* engine = new EngineCtx;
    memset(engine, 0, sizeof(*engine));
    engine->m_Headless = s_HeadlessMode;
    if (engine->m_Headless)
    {
        s_ScreenshotDelayFrames = 0;
    }

    WindowCreateParams window_params;
    WindowCreateParamsInitialize(&window_params);
    window_params.m_Width = 512;
    window_params.m_Height = 512;
    window_params.m_Title = "Rive Viewer App";

    window_params.m_GraphicsApi = GetWindowGraphicsApi(s_AdapterFamily);
    window_params.m_CloseCallback = OnWindowClose;
    window_params.m_CloseCallbackUserData = (void*)engine;
#if defined(__linux__)
    if (window_params.m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGL)
    {
        // Rive's OpenGL renderer requires at least OpenGL 4.2.
        window_params.m_OpenGLVersionHint = 42;
        window_params.m_OpenGLUseCoreProfileHint = 1;
    }
#endif

    engine->m_WasCreated++;
    engine->m_Running = 1;

    if (!engine->m_Headless)
    {
        engine->m_Window = WindowNew();
        if (!engine->m_Window)
        {
            dmLogWarning("Failed to create viewer window, running without graphics backend");
        }
        else
        {
            JobSystemCreateParams job_params = { 0 };
            job_params.m_ThreadCount = 1; // Needed for OpenGL graphics backend.
            engine->m_JobContext = JobSystemCreate(&job_params);

            if (!engine->m_JobContext)
            {
                dmLogWarning("Failed to create viewer job context, running without graphics backend");
            }
            else if (WindowOpen(engine->m_Window, &window_params) != WINDOW_RESULT_OK)
            {
                dmLogWarning("Failed to open viewer window, running without graphics backend");
            }
            else
            {
                WindowShow(engine->m_Window);

                dmGraphics::ContextParams graphics_context_params = {};
                graphics_context_params.m_DefaultTextureMinFilter = dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
                graphics_context_params.m_DefaultTextureMagFilter = dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
                graphics_context_params.m_VerifyGraphicsCalls = 1;
                graphics_context_params.m_UseValidationLayers = 1;
                graphics_context_params.m_Window = engine->m_Window;
                graphics_context_params.m_Width = 512;
                graphics_context_params.m_Height = 512;
                graphics_context_params.m_JobContext = engine->m_JobContext;
                graphics_context_params.m_PrintDeviceInfo = 1;

                if (window_params.m_GraphicsApi == WINDOW_GRAPHICS_API_VULKAN)
                {
                    graphics_context_params.m_GraphicsApiVersionMajorHint = 1;
                    graphics_context_params.m_GraphicsApiVersionMinorHint = 3;
                }

                engine->m_GraphicsContext = dmGraphics::NewContext(graphics_context_params);
                engine->m_GraphicsBackendAvailable = engine->m_GraphicsContext != 0;
                if (!engine->m_GraphicsBackendAvailable)
                {
                    dmLogWarning("Failed to create graphics context, running without graphics backend");
                }
            }
        }
    }
    else
    {
        dmLogInfo("Viewer headless mode enabled; skipping graphics backend creation");
    }

    if (engine->m_GraphicsBackendAvailable)
    {
        float bottom = 0.0f;
        float top = 1.0f;

        // Flip texture coordinates on y axis for OpenGL for the final blit:
        if (s_AdapterFamily != dmGraphics::ADAPTER_FAMILY_OPENGL)
        {
            top = 0.0f;
            bottom = 1.0f;
        }

        const float vertex_data[] = {
            -1.0f, -1.0f, 0.0f, bottom,  // Bottom-left corner
             1.0f, -1.0f, 1.0f, bottom,  // Bottom-right corner
            -1.0f,  1.0f, 0.0f, top,     // Top-left corner
             1.0f, -1.0f, 1.0f, bottom,  // Bottom-right corner
             1.0f,  1.0f, 1.0f, top,     // Top-right corner
            -1.0f,  1.0f, 0.0f, top      // Top-left corner
        };

        engine->m_BlitToBackbufferVertexBuffer = dmGraphics::NewVertexBuffer(engine->m_GraphicsContext, sizeof(vertex_data), (void*) vertex_data, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

        dmGraphics::HVertexStreamDeclaration stream_declaration_vertex = dmGraphics::NewVertexStreamDeclaration(engine->m_GraphicsContext);
        dmGraphics::AddVertexStream(stream_declaration_vertex, "position",  2, dmGraphics::TYPE_FLOAT, false);
        dmGraphics::AddVertexStream(stream_declaration_vertex, "texcoord0", 2, dmGraphics::TYPE_FLOAT, false);
        engine->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(engine->m_GraphicsContext, stream_declaration_vertex);
        dmGraphics::DeleteVertexStreamDeclaration(stream_declaration_vertex);

        dmGraphics::ShaderDesc* shader_desc = dmGraphics::CreateRiveModelBlitShaderDesc();
        char program_error[512] = {};
        engine->m_BlitProgram = dmGraphics::NewProgram(engine->m_GraphicsContext,
                                                       shader_desc,
                                                       program_error,
                                                       sizeof(program_error));
        engine->m_SamplerLocation = dmGraphics::INVALID_UNIFORM_LOCATION;
        if (engine->m_BlitProgram == dmGraphics::INVALID_PROGRAM_HANDLE || engine->m_BlitProgram == 0)
        {
            dmLogError("Failed to create blit program: %s", program_error);
            engine->m_BlitProgram = dmGraphics::INVALID_PROGRAM_HANDLE;
        }
        else
        {
            engine->m_SamplerLocation = dmGraphics::FindUniformLocation(engine->m_BlitProgram, "texture_sampler");
        }
    }

    // Rive
    engine->m_RenderContext = dmRive::NewRenderContext();
    if (!engine->m_RenderContext)
    {
        dmLogError("Failed to create Rive render context");
        return engine;
    }

    engine->m_RenderMutex = dmMutex::New();
    dmRive::SetRenderMutex(engine->m_RenderContext, engine->m_RenderMutex);

    dmRiveCommands::InitParams cmd_params = {};
    cmd_params.m_RenderContext = engine->m_RenderContext;
    cmd_params.m_Factory = dmRive::GetRiveFactory(engine->m_RenderContext);
    if (!cmd_params.m_Factory && !engine->m_GraphicsBackendAvailable)
    {
        engine->m_Factory = new ViewerNoOpFactory();
        cmd_params.m_Factory = engine->m_Factory;
    }
    cmd_params.m_Mutex = engine->m_RenderMutex;
    if (cmd_params.m_Factory)
    {
        dmRiveCommands::Initialize(&cmd_params);
    }
    else
    {
        dmLogError("Failed to get Rive factory");
    }

    if (s_RiveFilePath)
    {
        printf("Loading '%s'\n", s_RiveFilePath);

        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
        if (queue)
        {
            queue->setGlobalRenderImageListener(&s_RenderImageListener);
            std::vector<uint8_t> bytes;
            if (ReadFile(s_RiveFilePath, bytes))
            {
                engine->m_FileMeta = dmRive::LoadFileFromBuffer(bytes.data(), bytes.size(), s_RiveFilePath);
                if (engine->m_FileMeta)
                {
                    printf("Loaded file\n");

                    dmRive::DebugPrintFileState(engine->m_FileMeta);

                    engine->m_File = engine->m_FileMeta->m_File;
                    engine->m_Artboard = engine->m_FileMeta->m_Artboard;
                    engine->m_StateMachine = engine->m_FileMeta->m_StateMachine;
                    engine->m_ViewModelInstance = engine->m_FileMeta->m_ViewModelInstance;

                    bool has_view_models = true;
#if defined(DM_RIVE_FILE_META_DATA)
                    has_view_models = engine->m_FileMeta->m_ViewModels.Size() > 0;
#endif
                    if (has_view_models && engine->m_StateMachine && engine->m_ViewModelInstance)
                    {
                        queue->bindViewModelInstance(engine->m_StateMachine, engine->m_ViewModelInstance);
                    }
                }
            }

            engine->m_DrawKey = queue->createDrawKey();
        }
        else
        {
            dmLogError("Failed to get Rive command queue");
        }
    }

    return engine;
}

static void EngineDestroy(void* _engine)
{
    EngineCtx* engine = (EngineCtx*)_engine;

#if defined(DM_GRAPHICS_USE_VULKAN)
    if (engine->m_GraphicsContext)
    {
        VkDevice device = dmGraphics::VulkanGetDevice(engine->m_GraphicsContext);
        if (device != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(device);
        }
    }
#endif

    if (engine->m_FileMeta)
    {
        dmRive::DestroyFile(engine->m_FileMeta);
    }

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    if (queue)
    {
        queue->setGlobalRenderImageListener(0);
    }

    if (engine->m_RenderContext)
    {
        dmRiveCommands::Finalize();
        dmRive::SetRenderMutex(engine->m_RenderContext, 0);
        dmRive::DeleteRenderContext(engine->m_RenderContext);
    }
    if (engine->m_RenderMutex)
    {
        dmMutex::Delete(engine->m_RenderMutex);
    }
    if (engine->m_Factory)
    {
        delete engine->m_Factory;
        engine->m_Factory = 0;
    }

    if (engine->m_GraphicsContext)
    {
        dmGraphics::DeleteVertexBuffer(engine->m_BlitToBackbufferVertexBuffer);
        dmGraphics::DeleteVertexDeclaration(engine->m_VertexDeclaration);
        if (engine->m_BlitProgram != dmGraphics::INVALID_PROGRAM_HANDLE)
        {
            dmGraphics::DeleteProgram(engine->m_GraphicsContext, engine->m_BlitProgram);
        }
        dmGraphics::CloseWindow(engine->m_GraphicsContext);
        dmGraphics::DeleteContext(engine->m_GraphicsContext);
        dmGraphics::Finalize();
        engine->m_GraphicsContext = 0;
    }

    if (engine->m_Window)
    {
        WindowDelete(engine->m_Window);
        engine->m_Window = 0;
    }

    if (engine->m_JobContext)
    {
        JobSystemDestroy(engine->m_JobContext);
    }

    engine->m_WasDestroyed++;

    delete engine;
}

static void UpdateRiveScene(EngineCtx* engine)
{
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    if (!queue || engine->m_StateMachine == RIVE_NULL_HANDLE)
    {
        return;
    }

    static uint64_t last_update = dmTime::GetMonotonicTime();
    uint64_t time = dmTime::GetMonotonicTime();
    float dt = (time - last_update) / 1000000.0f;
    last_update = time;
    queue->advanceStateMachine(engine->m_StateMachine, dt);
}

static void DrawRiveScene(EngineCtx* engine)
{
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    if (!queue || !engine->m_GraphicsBackendAvailable || !engine->m_RenderContext || engine->m_Artboard == RIVE_NULL_HANDLE)
    {
        return;
    }

    rive::Renderer* renderer = dmRive::GetRiveRenderer(engine->m_RenderContext);

    const rive::ArtboardHandle  artboardHandle  = engine->m_Artboard;
    static rive::Fit       s_fit      = rive::Fit::contain;
    static rive::Alignment s_alignment= rive::Alignment::center;

    rive::Fit       fit = s_fit;
    rive::Alignment alignment = s_alignment;

    uint32_t width  = dmGraphics::GetWindowWidth(engine->m_GraphicsContext);
    uint32_t height = dmGraphics::GetWindowHeight(engine->m_GraphicsContext);
    float display_factor = dmGraphics::GetDisplayScaleFactor(engine->m_GraphicsContext);

    auto drawLoop = [artboardHandle,
                     renderer,
                     fit,
                     alignment,
                     width,
                     height,
                     display_factor](rive::DrawKey drawKey, rive::CommandServer* server)
    {
        rive::ArtboardInstance* artboard = server->getArtboardInstance(artboardHandle);
        if (artboard == nullptr)
        {
            return;
        }

        rive::Factory* factory = server->factory();
        bool fullscreen = false;
        if (fullscreen)
        {
            renderer->save();
            rive::AABB bounds = artboard->bounds();
            // // Apply the world matrix from the component to the artboard transform
            // rive::Mat2D transform         = rive::Mat2D::fromTranslate(width / 2.0f, height / 2.0f);
            // rive::Mat2D centerAdjustment  = rive::Mat2D::fromTranslate(-bounds.width() / 2.0f, -bounds.height() / 2.0f);
            // rive::Mat2D scaleDpi          = rive::Mat2D::fromScale(1,-1);
            // rive::Mat2D invertAdjustment  = rive::Mat2D::fromScaleAndTranslation(display_factor, -display_factor, 0, window_height);
            // rive::Mat2D rendererTransform = invertAdjustment * viewTransform * transform * scaleDpi * centerAdjustment;

            // renderer->transform(rendererTransform);
            // For making input work nicely
            //c->m_InverseRendererTransform = rendererTransform.invertOrIdentity();
            artboard->draw(renderer);
            renderer->restore();
        }
        else
        {
            rive::Mat2D renderer_transform = dmRive::CalcTransformRive(artboard,
                                                                        fit,
                                                                        alignment,
                                                                        width,
                                                                        height,
                                                                        display_factor);
            dmRive::DrawArtboard(artboard, renderer, renderer_transform);
        }
    };

    queue->draw(engine->m_DrawKey, drawLoop);
}

static UpdateResult EngineUpdate(void* _engine)
{
    EngineCtx* engine = (EngineCtx*)_engine;
    engine->m_WasRun++;
    uint64_t t = dmTime::GetMonotonicTime();

    if (!engine->m_Running)
    {
        return RESULT_EXIT;
    }

    if (engine->m_GraphicsBackendAvailable)
    {
        JobSystemUpdate(engine->m_JobContext, 0); // Flush any graphics jobs

        WindowPollEvents(engine->m_Window);

        if (engine->m_WindowClosed)
        {
            return RESULT_EXIT;
        }
    }

    UpdateRiveScene(engine);

    bool use_final_blit = true;
#if defined(DM_PLATFORM_LINUX) || defined(DM_GRAPHICS_USE_VULKAN)
    // Linux OpenGL can fail creating the intermediate render target.
    // Render directly to the default framebuffer in that case.

    use_final_blit = false;
#endif

    if (engine->m_GraphicsBackendAvailable && engine->m_RenderContext)
    {
        dmGraphics::BeginFrame(engine->m_GraphicsContext);

        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();

        dmRive::RenderBeginParams render_params;
        render_params.m_DoFinalBlit = use_final_blit;
        render_params.m_BackbufferSamples = 0;
        render_params.m_ClearColor = rive::colorARGB(255, 0, 0, 0);
        dmRive::RenderBegin(engine->m_RenderContext, 0, render_params);

        DrawRiveScene(engine);

        dmRiveCommands::ProcessMessages(); // Making sure any draw call is processed
        dmRive::RenderEnd(engine->m_RenderContext);
    }

    if (engine->m_GraphicsBackendAvailable && use_final_blit)
    {
        dmGraphics::Clear(engine->m_GraphicsContext, dmGraphics::BUFFER_TYPE_COLOR0_BIT, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0);
        DrawFullscreenQuad(engine, dmRive::GetBackingTexture(engine->m_RenderContext));
    }

    if (!s_ScreenshotCaptured && s_ScreenshotDelayFrames > 0)
    {
        --s_ScreenshotDelayFrames;
    }

    if (!s_ScreenshotCaptured && s_ScreenshotDelayFrames == 0)
    {
        printf("Capturing screenshot...\n");
        if (!engine->m_GraphicsBackendAvailable || !engine->m_RenderContext)
        {
            CaptureFallbackScreenshot(engine);
        }
        else
        {
            dmRive::TexturePixels pixels = {};
            bool read_ok = false;

#if defined(DM_GRAPHICS_USE_VULKAN)
            read_ok = dmRive::ReadPixelsBackBuffer(&pixels);
#else
            dmGraphics::HTexture backing_texture = dmRive::GetBackingTexture(engine->m_RenderContext);
            read_ok = dmRive::ReadPixels(backing_texture, &pixels);
#endif

            if (read_ok)
            {
                const char* screenshot_path = "./screenshot.tga";
                if (WriteTgaFile(screenshot_path, &pixels))
                {
                    printf("Wrote screenshot %s\n", screenshot_path);
                }
                else
                {
                    printf("Failed to write screenshot to %s\n", screenshot_path);
                }
                dmRive::FreePixels(&pixels);
            }
            else
            {
                printf("Failed to capture screenshot, writing fallback image instead\n");
                CaptureFallbackScreenshot(engine);
            }
        }
        s_ScreenshotCaptured = true;
    }

    if (engine->m_GraphicsBackendAvailable)
    {
        dmGraphics::Flip(engine->m_GraphicsContext);
    }
    else if (s_ScreenshotCaptured)
    {
        return RESULT_EXIT;
    }

    return RESULT_OK;
}

static void EngineGetResult(void* _engine, int* run_action, int* exit_code, int* argc, char*** argv)
{
    EngineCtx* ctx = (EngineCtx*)_engine;
    ctx->m_WasResultCalled++;
}

#if defined(DM_GRAPHICS_USE_VULKAN)
extern "C" void GraphicsAdapterVulkan();
#else
extern "C" void GraphicsAdapterOpenGL();
#endif

static void dmExportedSymbols()
{
#if defined(DM_GRAPHICS_USE_VULKAN)
    GraphicsAdapterVulkan();
#else
    GraphicsAdapterOpenGL();
#endif
}

int main(int argc, char** argv)
{
    dmExportedSymbols();
    s_AdapterFamily = dmGraphics::ADAPTER_FAMILY_VULKAN;
#if !defined(DM_GRAPHICS_USE_VULKAN)
    s_AdapterFamily = dmGraphics::ADAPTER_FAMILY_OPENGL;
#endif
    dmGraphics::InstallAdapter(s_AdapterFamily);

    for (int i = 1; i < argc; ++i)
    {
        const char* arg = argv[i];
        if (strcmp(arg, "--headless") == 0)
        {
            s_HeadlessMode = true;
        }
        else
        {
            size_t len = strlen(arg);
            if (len > 4 && strcmp(arg + len - 4, ".riv") == 0)
            {
                s_RiveFilePath = arg;
            }
            else
            {
                fprintf(stderr, "Unknown argument: %s\n", arg);
                return 1;
            }
        }
    }

    AppCtx ctx;
    memset(&ctx, 0, sizeof(ctx));

    RunLoopParams params = {};
    params.m_Argc = argc;
    params.m_Argv = argv;
    params.m_AppCtx = &ctx;
    params.m_AppCreate = AppCreate;
    params.m_AppDestroy = AppDestroy;
    params.m_EngineCreate = EngineCreate;
    params.m_EngineDestroy = EngineDestroy;
    params.m_EngineUpdate = EngineUpdate;
    params.m_EngineGetResult = EngineGetResult;

    return RunLoop(&params);
}
