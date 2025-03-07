#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/image.h>
#include <dmsdk/graphics/graphics_vulkan.h>

#include "renderer_context.h"

#include <rive/shapes/image.hpp>
#include <rive/renderer.hpp>
#include <rive/renderer/texture.hpp>

#include <rive/renderer/rive_render_image.hpp>

#include <defold/defold_graphics.h>
#include <defold/defold_render.h>

#include <defold/shaders/rivemodel_blit.spc.gen.h>

#include <common/vertices.h>
#include <defold/renderer.h>

#include <assert.h>

namespace dmResource
{
    void IncRef(HFactory factory, void* resource);
}

namespace dmRive
{
    struct DefoldRiveRenderer
    {
    #if defined(DM_PLATFORM_MACOS) || defined(DM_PLATFORM_IOS)
        IDefoldRiveRenderer* m_RenderContext = MakeDefoldRiveRendererMetal();
    #elif defined(DM_PLATFORM_WINDOWS)
        IDefoldRiveRenderer* m_RenderContext = MakeDefoldRiveRendererOpenGL();
    #elif defined(DM_PLATFORM_LINUX)
        IDefoldRiveRenderer* m_RenderContext = MakeDefoldRiveRendererOpenGL();
    #elif defined(DM_PLATFORM_ANDROID)
        IDefoldRiveRenderer* m_RenderContext = MakeDefoldRiveRendererOpenGL();
    #elif defined(DM_PLATFORM_HTML5)
        #ifdef RIVE_WEBGPU
            IDefoldRiveRenderer* m_RenderContext = MakeDefoldRiveRendererWebGPU();
        #else
            IDefoldRiveRenderer* m_RenderContext = MakeDefoldRiveRendererOpenGL();
        #endif
    #else
        #error "Platform not supported"
        assert(0 && "Platform not supported");
    #endif

        dmResource::HFactory m_Factory;
        rive::Renderer*      m_RiveRenderer;
        dmGraphics::HContext m_GraphicsContext;

        dmGraphics::HProgram m_BlitSpc;
        dmRender::HMaterial  m_BlitMaterial;

        uint32_t             m_LastWidth;
        uint32_t             m_LastHeight;
        uint8_t              m_LastDoFinalBlit : 1;
        uint8_t              m_FrameBegin : 1;
    };

    static DefoldRiveRenderer* g_RiveRenderer = 0;

    HRenderContext NewRenderContext()
    {
        if (g_RiveRenderer == 0)
        {
            g_RiveRenderer = new DefoldRiveRenderer();
            g_RiveRenderer->m_RiveRenderer    = 0;
            g_RiveRenderer->m_GraphicsContext = 0;
            g_RiveRenderer->m_LastWidth       = 0;
            g_RiveRenderer->m_LastHeight      = 0;
            g_RiveRenderer->m_FrameBegin      = 0;
        }

        return (HRenderContext) g_RiveRenderer;
    }

    static void AddShaderResources(dmResource::HFactory factory)
    {
        dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/rivemodel_blit.spc", RIVEMODEL_BLIT_SPC_SIZE, RIVEMODEL_BLIT_SPC);
    }

    static void RemoveShaderResources(dmResource::HFactory factory)
    {
        dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/rivemodel_blit.spc");
    }

    dmResource::Result LoadShaders(dmResource::HFactory factory, ShaderResources** resources)
    {
        dmResource::Result result = dmResource::RESULT_OK;

        AddShaderResources(factory);

        #define GET_SHADER(path, storage) \
            result = dmResource::Get(factory, path, (void**) &storage); \
            if (result != dmResource::RESULT_OK) \
            { \
                dmLogError("Failed to load resource '%s'", path); \
                return result; \
            }

        GET_SHADER("/defold-rive/assets/pls-shaders/rivemodel_blit.spc", g_RiveRenderer->m_BlitSpc);

        #undef GET_SHADER

        RemoveShaderResources(factory);

        return result;
    }

    static void ReleaseShadersInternal(dmResource::HFactory factory)
    {
        #define RELEASE_SHADER(res) \
            if (res) dmResource::Release(factory, (void*) res);
        RELEASE_SHADER(g_RiveRenderer->m_BlitSpc);
        #undef RELEASE_SHADER
    }

    void ReleaseShaders(dmResource::HFactory factory, ShaderResources** resources)
    {
        ReleaseShadersInternal(factory);
        *resources = 0;
    }

    void DeleteRenderContext(HRenderContext context)
    {
        if (g_RiveRenderer)
        {
            ReleaseShadersInternal(g_RiveRenderer->m_Factory);
            delete g_RiveRenderer;
            g_RiveRenderer = 0;
        }
    }

    rive::Factory* GetRiveFactory(HRenderContext context)
    {
        DefoldRiveRenderer* renderer = (DefoldRiveRenderer*) context;
        return renderer->m_RenderContext->Factory();
    }

    rive::Renderer* GetRiveRenderer(HRenderContext context)
    {
        DefoldRiveRenderer* renderer = (DefoldRiveRenderer*) context;
        return renderer->m_RiveRenderer;
    }

    dmGraphics::HTexture GetBackingTexture(HRenderContext context)
    {
        DefoldRiveRenderer* renderer = (DefoldRiveRenderer*) context;
        return renderer->m_RenderContext->GetBackingTexture();
    }

    void RenderBegin(HRenderContext context, dmResource::HFactory factory, const RenderBeginParams& params)
    {
        DefoldRiveRenderer* renderer = (DefoldRiveRenderer*) context;

        if (!renderer->m_RiveRenderer)
        {
            renderer->m_GraphicsContext = dmGraphics::GetInstalledContext();
            renderer->m_RenderContext->SetGraphicsContext(renderer->m_GraphicsContext);
            renderer->m_RiveRenderer = renderer->m_RenderContext->MakeRenderer();
            renderer->m_Factory = factory;

            dmResource::IncRef(factory, (void*) renderer->m_BlitSpc);
        }

        if (!renderer->m_FrameBegin)
        {
            uint32_t width  = dmGraphics::GetWindowWidth(renderer->m_GraphicsContext);
            uint32_t height = dmGraphics::GetWindowHeight(renderer->m_GraphicsContext);

            if (width != renderer->m_LastWidth || height != renderer->m_LastHeight || renderer->m_LastDoFinalBlit != params.m_DoFinalBlit)
            {
                dmLogInfo("Change size to %d, %d", width, height);
                renderer->m_RenderContext->OnSizeChanged(width, height, params.m_BackbufferSamples, params.m_DoFinalBlit);
                renderer->m_LastWidth  = width;
                renderer->m_LastHeight = height;
                renderer->m_LastDoFinalBlit = params.m_DoFinalBlit;
            }

            int samples = (int) params.m_DoFinalBlit ? 0 : params.m_BackbufferSamples;

        #if defined(DM_PLATFORM_MACOS) || defined(DM_PLATFORM_IOS)
            dmGraphics::HTexture swap_chain_texture = dmGraphics::VulkanGetActiveSwapChainTexture(renderer->m_GraphicsContext);
            renderer->m_RenderContext->SetRenderTargetTexture(swap_chain_texture);
            samples = 0;
        #endif

            renderer->m_RenderContext->BeginFrame({
                .renderTargetWidth      = width,
                .renderTargetHeight     = height,
                .clearColor             = 0x00000000,
                .msaaSampleCount        = 0,
                // .msaaSampleCount        = samples,
                // .disableRasterOrdering  = s_forceAtomicMode,
                // .wireframe              = s_wireframe,
                // .fillsDisabled          = s_disableFill,
                // .strokesDisabled        = s_disableStroke,
            });

            renderer->m_FrameBegin = 1;
        }
    }

    void GetDimensions(HRenderContext context, uint32_t* width, uint32_t* height)
    {
        DefoldRiveRenderer* renderer = (DefoldRiveRenderer*) context;
        *width = renderer->m_LastWidth;
        *height = renderer->m_LastHeight;
    }

    void RenderEnd(HRenderContext context)
    {
        DefoldRiveRenderer* renderer = (DefoldRiveRenderer*) context;

        if (renderer->m_FrameBegin)
        {
            renderer->m_RenderContext->Flush();
            renderer->m_FrameBegin = 0;
        }
    }

    static void RepackLuminanceToRGBA(uint32_t num_pixels, uint8_t* luminance, uint8_t* rgba)
    {
        for(uint32_t px=0; px < num_pixels; px++)
        {
            rgba[0] = luminance[0];
            rgba[1] = 0;
            rgba[2] = 0;
            rgba[3] = 255;
            rgba+=4;
            luminance++;
        }
    }

    static void RepackLuminanceAlphaToRGBA(uint32_t num_pixels, uint8_t* luminance, uint8_t* rgba)
    {
        for(uint32_t px=0; px < num_pixels; px++)
        {
            rgba[0] = luminance[0];
            rgba[1] = luminance[0];
            rgba[2] = luminance[0];
            rgba[3] = luminance[1];
            rgba+=4;
            luminance+=2;
        }
    }

    rive::rcp<rive::RenderImage> CreateRiveRenderImage(HRenderContext context, void* bytes, uint32_t byte_count)
    {
        dmImage::HImage img          = dmImage::NewImage(bytes, byte_count, false);
        DefoldRiveRenderer* renderer = (DefoldRiveRenderer*) context;

        rive::rcp<rive::gpu::Texture> texture;
        if (img)
        {
            dmImage::Type img_type = dmImage::GetType(img);
            uint32_t img_width = dmImage::GetWidth(img);
            uint32_t img_height = dmImage::GetHeight(img);

            uint8_t* bitmap_data_rgba = (uint8_t*) dmImage::GetData(img);
            uint8_t* bitmap_data_tmp  = 0;

            if (img_type == dmImage::TYPE_RGB)
            {
                bitmap_data_tmp = (uint8_t*) malloc(img_width * img_height * 4);

                dmGraphics::RepackRGBToRGBA(img_width * img_height, bitmap_data_rgba, bitmap_data_tmp);

                bitmap_data_rgba = bitmap_data_tmp;
            }
            else if (img_type == dmImage::TYPE_LUMINANCE)
            {
                bitmap_data_tmp = (uint8_t*) malloc(img_width * img_height * 4);

                RepackLuminanceToRGBA(img_width * img_height, bitmap_data_rgba, bitmap_data_tmp);

                bitmap_data_rgba = bitmap_data_tmp;
            }
            else if (img_type == dmImage::TYPE_LUMINANCE_ALPHA)
            {
                bitmap_data_tmp = (uint8_t*) malloc(img_width * img_height * 4);

                RepackLuminanceAlphaToRGBA(img_width * img_height, bitmap_data_rgba, bitmap_data_tmp);

                bitmap_data_rgba = bitmap_data_tmp;
            }

            texture = renderer->m_RenderContext->MakeImageTexture(img_width, img_height, 0, (const uint8_t*) bitmap_data_rgba);

            dmImage::DeleteImage(img);

            if (bitmap_data_tmp)
            {
                free(bitmap_data_tmp);
            }
        }
        else
        {
            if (byte_count >= 16)
            {
                const char* header = (const char*)bytes;
                for (int i = 0; i < 16-4; ++i)
                {
                    if (header[i+0] == 'W' && header[i+1] == 'E' && header[i+2] == 'B' && header[i+3] == 'P')
                    {
                        dmLogError("We don't currently support images in WEBP format");
                        break;
                    }
                }
            }
            uint8_t pink[] = {227, 61, 148, 255};
            texture = renderer->m_RenderContext->MakeImageTexture(1, 1, 0, (const uint8_t*) pink);
        }

        return texture != nullptr ? rive::make_rcp<rive::RiveRenderImage>(std::move(texture)) : nullptr;
    }

    dmRender::HMaterial GetBlitToBackBufferMaterial(HRenderContext context, dmRender::HRenderContext render_context)
    {
        DefoldRiveRenderer* renderer = (DefoldRiveRenderer*) context;
        if (!renderer->m_BlitMaterial)
        {
            renderer->m_BlitMaterial = dmRender::NewMaterial(render_context, renderer->m_BlitSpc);

            if (!dmRender::SetMaterialSampler(renderer->m_BlitMaterial, dmHashString64("texture_sampler"), 0, dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE, dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE, dmGraphics::TEXTURE_FILTER_LINEAR, dmGraphics::TEXTURE_FILTER_LINEAR, 1.0))
            {
                dmLogError("Failed to set material sampler");
            }

            dmhash_t dummy_tag = dmHashString64("rive");
            dmRender::SetMaterialTags(renderer->m_BlitMaterial, 1, &dummy_tag);
        }
        assert(renderer->m_BlitMaterial);
        return renderer->m_BlitMaterial;
    }

    rive::Mat2D GetViewTransform(HRenderContext context, dmRender::HRenderContext render_context)
    {
        const dmVMath::Matrix4& view_matrix = dmRender::GetViewMatrix(render_context);
        rive::Mat2D viewTransform;
        Mat4ToMat2D(view_matrix, viewTransform);
        return viewTransform;
    }
}
