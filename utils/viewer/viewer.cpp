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
//#include <render/render.h>

#include <defold/rive.h>
#include <common/commands.h>

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

    // Rive related
    dmRive::HRenderContext m_RenderContext;

    rive::FileHandle            m_File;
    rive::ArtboardHandle        m_Artboard;
    rive::StateMachineHandle    m_StateMachine;
    rive::DrawKey               m_DrawKey;
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
            engine->m_File = queue->loadFile(bytes);

            if (engine->m_File)
            {
                printf("Loaded file\n");

                engine->m_Artboard = queue->instantiateDefaultArtboard(engine->m_File);
                if (engine->m_Artboard)
                {
                    printf("Created default artboard\n");

                    engine->m_StateMachine = queue->instantiateDefaultStateMachine(engine->m_Artboard);
                    if (engine->m_StateMachine)
                    {
                        printf("Created default state machine\n");
                    }
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

    dmRiveCommands::Finalize();
    dmRive::DeleteRenderContext(engine->m_RenderContext);

    dmJobThread::Destroy(engine->m_JobThread);

    dmGraphics::DeleteVertexBuffer(engine->m_BlitToBackbufferVertexBuffer);
    dmGraphics::DeleteVertexDeclaration(engine->m_VertexDeclaration);

    dmGraphics::CloseWindow(engine->m_GraphicsContext);
    dmGraphics::DeleteContext(engine->m_GraphicsContext);
    dmGraphics::Finalize();

    engine->m_WasDestroyed++;

    delete engine;
}

static void DrawRiveScene(EngineCtx* engine)
{
    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();

    // engine->m_Factory
    dmRive::RenderBeginParams render_params;
    render_params.m_DoFinalBlit = true;
    render_params.m_BackbufferSamples = 0;
    RenderBegin(engine->m_RenderContext, 0, render_params);

    rive::Renderer* renderer = dmRive::GetRiveRenderer(engine->m_RenderContext);

    const rive::ArtboardHandle  artboardHandle  = engine->m_Artboard;
    rive::Fit                   fit             = rive::Fit::none;
    rive::Alignment             alignment       = rive::Alignment::center;


    uint32_t width  = dmGraphics::GetWidth(engine->m_GraphicsContext);
    uint32_t height = dmGraphics::GetHeight(engine->m_GraphicsContext);
    //g_DisplayFactor        = dmGraphics::GetDisplayScaleFactor(rivectx->m_GraphicsContext);


    auto drawLoop = [artboardHandle,
                     renderer,
                     fit,
                     alignment,
                     width,
                     height](rive::DrawKey drawKey, rive::CommandServer* server)
    {
        rive::ArtboardInstance* artboard = server->getArtboardInstance(artboardHandle);
        if (artboard == nullptr)
        {
            return;
        }

        rive::Factory* factory = server->factory();

        if (fit == rive::Fit::layout)
        {
            artboard->width(width);
            artboard->height(height);
        }

        // Draw the .riv.
        renderer->save();
        renderer->align(fit,
                        alignment,
                        rive::AABB(0, 0, width, height),
                        artboard->bounds());

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

    // bool do_render = !dmRender::IsRenderPaused(engine->m_RenderListContext);

    // if (do_render)
    // {
    //     dmRender::RenderListBegin(engine->m_RenderListContext);
    // }

    DrawRiveScene(engine);
    dmRiveCommands::ProcessMessages(); // Making sure any draw call is processed

    // if (do_render)
    // {
    //     dmRender::RenderListEnd(engine->m_RenderListContext);
    // }

    dmGraphics::BeginFrame(engine->m_GraphicsContext);
    dmGraphics::Clear(engine->m_GraphicsContext, dmGraphics::BUFFER_TYPE_COLOR0_BIT,
                                            0.0f, 0.0f, 0.0f, 0.0f,
                                            1.0f, 0);

    // if (do_render)
    // {
    //     dmRender::DrawRenderList(engine->m_RenderListContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);
    //     dmRender::ClearRenderObjects(engine->m_RenderListContext);
    // }

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
