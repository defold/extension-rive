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
#include <stdio.h>
#include <string.h>
#include <vector>

#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/time.h>
#include <dmsdk/graphics/graphics.h>
#include <dmsdk/render/render.h>


#include <platform/platform_window.h>
#include <dlib/log.h> // LogParams
#include <dlib/job_thread.h> // JobThread
#include <graphics/graphics.h> // ContextParams

#include <defold/rive.h>
#include "file.h"
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


static const char* s_RiveFilePath = 0;

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
    int m_WasResultCalled;
    int m_Running;
    bool m_WindowClosed;

    dmJobThread::HContext m_JobThread;

    dmPlatform::HWindow                 m_Window;
    dmGraphics::HContext                m_GraphicsContext;

    dmGraphics::HVertexBuffer           m_BlitToBackbufferVertexBuffer;
    dmGraphics::HVertexDeclaration      m_VertexDeclaration;
    dmGraphics::HProgram                m_BlitProgram;
    dmGraphics::HTexture                m_Texture;
    dmGraphics::HUniformLocation        m_SamplerLocation;

    // Rive related
    dmRive::HRenderContext m_RenderContext;
    dmRive::FileMeta::RiveFile* m_FileMeta;

    rive::FileHandle                m_File;
    rive::ArtboardHandle            m_Artboard;
    rive::StateMachineHandle        m_StateMachine;
    rive::ViewModelInstanceHandle   m_ViewModelInstance;
    rive::DrawKey                   m_DrawKey;
};

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
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);

    AppCtx* ctx = (AppCtx*)_ctx;
    ctx->m_Created++;
}

static void AppDestroy(void* _ctx)
{
    AppCtx* ctx = (AppCtx*)_ctx;
    ctx->m_Destroyed++;
}

static dmGraphics::HUniformLocation FindUniformLocation(dmGraphics::HProgram program, const char* name)
{
    uint32_t uniform_count = dmGraphics::GetUniformCount(program);
    for (uint32_t i = 0; i < uniform_count; ++i)
    {
        dmGraphics::Uniform uniform;
        dmGraphics::GetUniform(program, i, &uniform);
        if (uniform.m_Name && strcmp(uniform.m_Name, name) == 0)
        {
            return uniform.m_Location;
        }
    }

    return dmGraphics::INVALID_UNIFORM_LOCATION;
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
    dmGraphics::EnableVertexDeclaration(context, engine->m_VertexDeclaration, 0, 0, engine->m_BlitProgram);
    dmGraphics::EnableVertexBuffer(context, engine->m_BlitToBackbufferVertexBuffer, 0);

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

static bool OnWindowClose(void* user_data)
{
    EngineCtx* engine = (EngineCtx*) user_data;
    engine->m_WindowClosed = 1;
    return true;
}

static void* EngineCreate(int argc, char** argv)
{
    EngineCtx* engine = new EngineCtx;
    memset(engine, 0, sizeof(*engine));
    engine->m_Window = dmPlatform::NewWindow();

    dmJobThread::JobThreadCreationParams job_params = {0};
    engine->m_JobThread = dmJobThread::Create(job_params);

    dmPlatform::WindowParams window_params = {};
    window_params.m_Width                  = 512;
    window_params.m_Height                 = 512;
    window_params.m_Title                  = "Rive Viewer App";

    window_params.m_GraphicsApi            = dmPlatform::PLATFORM_GRAPHICS_API_VULKAN;
    window_params.m_CloseCallback          = OnWindowClose;
    window_params.m_CloseCallbackUserData  = (void*) engine;

    if (dmGraphics::GetInstalledAdapterFamily() == dmGraphics::ADAPTER_FAMILY_OPENGL)
    {
        window_params.m_GraphicsApi = dmPlatform::PLATFORM_GRAPHICS_API_OPENGL;
    }
    else if (dmGraphics::GetInstalledAdapterFamily() == dmGraphics::ADAPTER_FAMILY_OPENGLES)
    {
        window_params.m_GraphicsApi = dmPlatform::PLATFORM_GRAPHICS_API_OPENGLES;
    }
    else if (dmGraphics::GetInstalledAdapterFamily() == dmGraphics::ADAPTER_FAMILY_DIRECTX)
    {
        window_params.m_GraphicsApi = dmPlatform::PLATFORM_GRAPHICS_API_DIRECTX;
    }

    dmPlatform::OpenWindow(engine->m_Window, window_params);
    dmPlatform::ShowWindow(engine->m_Window);

    dmGraphics::ContextParams graphics_context_params = {};
    graphics_context_params.m_DefaultTextureMinFilter = dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
    graphics_context_params.m_DefaultTextureMagFilter = dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
    graphics_context_params.m_VerifyGraphicsCalls     = 1;
    graphics_context_params.m_UseValidationLayers     = 1;
    graphics_context_params.m_Window                  = engine->m_Window;
    graphics_context_params.m_Width                   = 512;
    graphics_context_params.m_Height                  = 512;
    graphics_context_params.m_JobThread               = engine->m_JobThread;

    engine->m_GraphicsContext = dmGraphics::NewContext(graphics_context_params);

    engine->m_WasCreated++;
    engine->m_Running = 1;

    // dmRender::RenderContextParams render_params;
    // render_params.m_ScriptContext = 0;
    // render_params.m_SystemFontMap = 0;
    // render_params.m_ShaderProgramDesc = 0;
    // render_params.m_MaxRenderTypes = 16;
    // render_params.m_MaxInstances = 2048;
    // render_params.m_MaxRenderTargets = 32;
    // render_params.m_ShaderProgramDescSize = 0;
    // render_params.m_MaxCharacters = 2048 * 4;
    // render_params.m_MaxBatches = 128;
    // render_params.m_CommandBufferSize = 1024;
    // render_params.m_MaxDebugVertexCount = 0;
    // engine->m_RenderListContext = dmRender::NewRenderContext(engine->m_GraphicsContext, render_params);

    // Graphics

    float bottom = 0.0f;
    float top    = 1.0f;

    // Flip texture coordinates on y axis for OpenGL for the final blit:
    if (dmGraphics::GetInstalledAdapterFamily() != dmGraphics::ADAPTER_FAMILY_OPENGL)
    {
        top    = 0.0f;
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
        engine->m_SamplerLocation = FindUniformLocation(engine->m_BlitProgram, "texture_sampler");
    }

    // Rive
    engine->m_RenderContext = dmRive::NewRenderContext();

    dmRiveCommands::InitParams cmd_params;
    cmd_params.m_RenderContext = engine->m_RenderContext;
    cmd_params.m_Factory = dmRive::GetRiveFactory(engine->m_RenderContext);
    dmRiveCommands::Initialize(&cmd_params);

    if (s_RiveFilePath)
    {
        printf("Loading '%s'\n", s_RiveFilePath);

        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
        std::vector<uint8_t> bytes;
        if (ReadFile(s_RiveFilePath, bytes))
        {
            engine->m_FileMeta = dmRive::FileMeta::LoadFileFromBuffer(bytes.data(), bytes.size(), s_RiveFilePath);
            if (engine->m_FileMeta)
            {
                printf("Loaded file\n");

                dmRive::FileMeta::DebugPrintFileState(engine->m_FileMeta);

                engine->m_File = engine->m_FileMeta->m_File;
                engine->m_Artboard = engine->m_FileMeta->m_Artboard;
                engine->m_StateMachine = engine->m_FileMeta->m_StateMachine;
                engine->m_ViewModelInstance = engine->m_FileMeta->m_ViewModelInstance;

                if (engine->m_StateMachine && engine->m_ViewModelInstance)
                {
                    queue->bindViewModelInstance(engine->m_StateMachine, engine->m_ViewModelInstance);
                }
            }
        }

        engine->m_DrawKey = queue->createDrawKey();
    }

    return engine;
}

static void EngineDestroy(void* _engine)
{
    EngineCtx* engine = (EngineCtx*)_engine;

    if (engine->m_FileMeta)
    {
        dmRive::FileMeta::DestroyFile(engine->m_FileMeta);
    }

    dmRiveCommands::Finalize();
    dmRive::DeleteRenderContext(engine->m_RenderContext);

    dmJobThread::Destroy(engine->m_JobThread);

    dmGraphics::DeleteVertexBuffer(engine->m_BlitToBackbufferVertexBuffer);
    dmGraphics::DeleteVertexDeclaration(engine->m_VertexDeclaration);
    if (engine->m_BlitProgram != 0 && engine->m_BlitProgram != dmGraphics::INVALID_PROGRAM_HANDLE)
    {
        dmGraphics::DeleteProgram(engine->m_GraphicsContext, engine->m_BlitProgram);
    }

    dmGraphics::CloseWindow(engine->m_GraphicsContext);
    dmGraphics::DeleteContext(engine->m_GraphicsContext);
    dmGraphics::Finalize();

    engine->m_WasDestroyed++;

    delete engine;
}

static void UpdateRiveScene(EngineCtx* engine)
{
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    if (engine->m_StateMachine == RIVE_NULL_HANDLE)
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
    if (engine->m_Artboard == RIVE_NULL_HANDLE)
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
        // Draw the .riv.
        renderer->save();

        rive::AABB bounds = artboard->bounds();

        bool fullscreen = false;
        if (fullscreen)
        {
            // // Apply the world matrix from the component to the artboard transform
            // rive::Mat2D transform         = rive::Mat2D::fromTranslate(width / 2.0f, height / 2.0f);
            // rive::Mat2D centerAdjustment  = rive::Mat2D::fromTranslate(-bounds.width() / 2.0f, -bounds.height() / 2.0f);
            // rive::Mat2D scaleDpi          = rive::Mat2D::fromScale(1,-1);
            // rive::Mat2D invertAdjustment  = rive::Mat2D::fromScaleAndTranslation(display_factor, -display_factor, 0, window_height);
            // rive::Mat2D rendererTransform = invertAdjustment * viewTransform * transform * scaleDpi * centerAdjustment;

            // renderer->transform(rendererTransform);
            // For making input work nicely
            //c->m_InverseRendererTransform = rendererTransform.invertOrIdentity();
        }
        else
        {
            if (fit == rive::Fit::layout)
            {
                artboard->width(width / display_factor);
                artboard->height(height / display_factor);
            }

            rive::Mat2D rendererTransform = rive::computeAlignment(fit, alignment, rive::AABB(0, 0, width, height), bounds, display_factor);
            renderer->transform(rendererTransform);
            // For making input work nicely
            //c->m_InverseRendererTransform = rendererTransform.invertOrIdentity();
        }

        artboard->draw(renderer);
        renderer->restore();
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

    dmJobThread::Update(engine->m_JobThread, 0); // Flush any graphics jobs

    dmPlatform::PollEvents(engine->m_Window);

    if (engine->m_WindowClosed)
    {
        return RESULT_EXIT;
    }

    UpdateRiveScene(engine);

    {
        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();

        // engine->m_Factory
        dmRive::RenderBeginParams render_params;
        render_params.m_DoFinalBlit = true;
        render_params.m_BackbufferSamples = 0;
        dmRive::RenderBegin(engine->m_RenderContext, 0, render_params);

        DrawRiveScene(engine);

        dmRiveCommands::ProcessMessages(); // Making sure any draw call is processed
        dmRive::RenderEnd(engine->m_RenderContext);
    }

    dmGraphics::BeginFrame(engine->m_GraphicsContext);
    dmGraphics::Clear(engine->m_GraphicsContext, dmGraphics::BUFFER_TYPE_COLOR0_BIT,
                                            0.0f, 0.0f, 0.0f, 0.0f,
                                            1.0f, 0);

    DrawFullscreenQuad(engine, dmRive::GetBackingTexture(engine->m_RenderContext));

    dmGraphics::Flip(engine->m_GraphicsContext);

    return RESULT_OK;
}

static void EngineGetResult(void* _engine, int* run_action, int* exit_code, int* argc, char*** argv)
{
    EngineCtx* ctx = (EngineCtx*)_engine;
    ctx->m_WasResultCalled++;
}

#if defined(__APPLE__) || defined(__linux__)
extern "C" void GraphicsAdapterVulkan();
#else
extern "C" void GraphicsAdapterOpenGL();
#endif

static void dmExportedSymbols()
{
#if defined(__APPLE__) || defined(__linux__)
    GraphicsAdapterVulkan();
#else
    GraphicsAdapterOpenGL();
#endif
}

int main(int argc, char **argv)
{
    dmExportedSymbols();
    dmGraphics::InstallAdapter();

    if (argc > 1)
    {
        const char* path = argv[argc-1];
        size_t len = strlen(path);
        if (len > 4 && strcmp(path + len - 4, ".riv") == 0)
        {
           s_RiveFilePath = path;
        }
        else
        {
            fprintf(stderr, "Must speficy a .riv path");
            return 1;
        }
    }

    AppCtx ctx;
    memset(&ctx, 0, sizeof(ctx));

    RunLoopParams params;
    params.m_AppCtx = &ctx;
    params.m_AppCreate = AppCreate;
    params.m_AppDestroy = AppDestroy;
    params.m_EngineCreate = EngineCreate;
    params.m_EngineDestroy = EngineDestroy;
    params.m_EngineUpdate = EngineUpdate;
    params.m_EngineGetResult = EngineGetResult;

    return RunLoop(&params);
}
