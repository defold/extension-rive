#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/image.h>
#include <dmsdk/graphics/graphics_vulkan.h> // REMOVE

#include "renderer_context.h"

#include <rive/shapes/image.hpp>
#include <rive/renderer.hpp>
#include <rive/renderer/texture.hpp>

#include <rive/renderer/rive_render_image.hpp>

#include <private/defold_graphics.h>

#include <common/vertices.h>
#include <renderer.h>

#include <assert.h>

namespace dmResource
{
    void IncRef(HFactory factory, void* resource);
}

namespace dmGraphics
{
    void     RepackRGBToRGBA(uint32_t num_pixels, uint8_t* rgb, uint8_t* rgba);
    HContext GetInstalledContext();
}

namespace dmRender
{
    const dmVMath::Matrix4& GetViewMatrix(HRenderContext render_context);
    const dmVMath::Matrix4& GetViewProjectionMatrix(HRenderContext render_context);
}

// DM_RIVE_USE_OPENGL

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
        uint32_t             m_LastWidth;
        uint32_t             m_LastHeight;
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
    }

    static void RemoveShaderResources(dmResource::HFactory factory)
    {
    }

    dmResource::Result LoadShaders(dmResource::HFactory factory, ShaderResources** resources)
    {
        return dmResource::RESULT_OK;
    }

    static void ReleaseShadersInternal(dmResource::HFactory factory, ShaderResources* shaders)
    {
    }

    void ReleaseShaders(dmResource::HFactory factory, ShaderResources** resources)
    {
    }

    void DeleteRenderContext(HRenderContext context)
    {
        if (g_RiveRenderer)
        {
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

    void RenderBegin(HRenderContext context, ShaderResources* shaders, dmResource::HFactory factory)
    {
        DefoldRiveRenderer* renderer = (DefoldRiveRenderer*) context;

        if (!renderer->m_RiveRenderer)
        {
            renderer->m_GraphicsContext = dmGraphics::GetInstalledContext();
            renderer->m_RenderContext->SetGraphicsContext(renderer->m_GraphicsContext);
            renderer->m_RiveRenderer = renderer->m_RenderContext->MakeRenderer();
        }

        if (!renderer->m_FrameBegin)
        {
            uint32_t width  = dmGraphics::GetWindowWidth(renderer->m_GraphicsContext);
            uint32_t height = dmGraphics::GetWindowHeight(renderer->m_GraphicsContext);

            uint32_t window_width = width;
            uint32_t window_height = height;

            // uint32_t fb_width  = dmGraphics::GetWindowWidth(renderer->m_GraphicsContext);
            // uint32_t fb_height = dmGraphics::GetWindowHeight(renderer->m_GraphicsContext);

            // dmLogInfo("Window=(%d,%d), FB=(%d,%d)", fb_width, fb_height, width, height);
            // width = window_width;
            // height = window_height;

            int32_t msaa_samples = 0;

            if (width != renderer->m_LastWidth || height != renderer->m_LastHeight)
            {
                dmLogInfo("Change size to %d, %d", width, height);
                renderer->m_RenderContext->OnSizeChanged(window_width, window_height, msaa_samples);
                renderer->m_LastWidth  = width;
                renderer->m_LastHeight = height;
            }

        #if defined(DM_PLATFORM_MACOS) || defined(DM_PLATFORM_IOS)
            dmGraphics::HTexture swap_chain_texture = dmGraphics::VulkanGetActiveSwapChainTexture(renderer->m_GraphicsContext);
            renderer->m_RenderContext->SetRenderTargetTexture(swap_chain_texture);
        #endif

            renderer->m_RenderContext->BeginFrame({
                .renderTargetWidth      = window_width,
                .renderTargetHeight     = window_height,
                .clearColor             = 0x00000000,
                .msaaSampleCount        = msaa_samples,
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

    rive::Mat2D GetViewTransform(HRenderContext context, dmRender::HRenderContext render_context)
    {
        const dmVMath::Matrix4& view_matrix = dmRender::GetViewMatrix(render_context);
        rive::Mat2D viewTransform;
        Mat4ToMat2D(view_matrix, viewTransform);
        return viewTransform;
    }
}
