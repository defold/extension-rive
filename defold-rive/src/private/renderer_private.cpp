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
    #elif defined(DM_PLATFORM_ANDROID) || defined(DM_PLATFORM_HTML5)
        IDefoldRiveRenderer* m_RenderContext = MakeDefoldRiveRendererOpenGL();
    #else
        #error "Platform not supported"
        assert(0 && "Platform not supported");
    #endif

        /*
    #if defined(DM_PLATFORM_MACOS) || defined(DM_PLATFORM_IOS)
        std::unique_ptr<rive::pls::PLSRenderContext> m_PLSRenderContext = DefoldPLSRenderContext::MakeContext(CONFIG_SUB_PASS_LOAD);
    #elif defined(DM_PLATFORM_WINDOWS) || defined(DM_PLATFORM_LINUX) || defined(DM_PLATFORM_ANDROID) || defined(DM_PLATFORM_SWITCH)
        std::unique_ptr<rive::pls::PLSRenderContext> m_PLSRenderContext = DefoldPLSRenderContext::MakeContext(CONFIG_RW_TEXTURE);
    #else
        #error "Platform not supported"
        assert(0 && "Platform not supported");
    #endif
        */

        // ShaderResources                              m_Shaders;
        dmResource::HFactory                         m_Factory;
        rive::Renderer*                              m_RiveRenderer;
        dmGraphics::HContext                         m_GraphicsContext;
        uint32_t                                     m_LastWidth;
        uint32_t                                     m_LastHeight;
        uint8_t                                      m_FrameBegin : 1;
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
        // dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/color_ramp.vpc",              COLOR_RAMP_VPC_SIZE, COLOR_RAMP_VPC);
        // dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/color_ramp.fpc",              COLOR_RAMP_FPC_SIZE, COLOR_RAMP_FPC);
        // dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/tessellate.vpc",              TESSELLATE_VPC_SIZE, TESSELLATE_VPC);
        // dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/tessellate.fpc",              TESSELLATE_FPC_SIZE, TESSELLATE_FPC);
        // dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/draw_path.vpc",               DRAW_PATH_VPC_SIZE, DRAW_PATH_VPC);
        // dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/draw_path.fpc",               DRAW_PATH_FPC_SIZE, DRAW_PATH_FPC);
        // dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/draw_interior_triangles.vpc", DRAW_INTERIOR_TRIANGLES_VPC_SIZE, DRAW_INTERIOR_TRIANGLES_VPC);
        // dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/draw_interior_triangles.fpc", DRAW_INTERIOR_TRIANGLES_FPC_SIZE, DRAW_INTERIOR_TRIANGLES_FPC);
        // dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/draw_image_mesh.vpc",         DRAW_IMAGE_MESH_VPC_SIZE, DRAW_IMAGE_MESH_VPC);
        // dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/draw_image_mesh.fpc",         DRAW_IMAGE_MESH_FPC_SIZE, DRAW_IMAGE_MESH_FPC);
    }

    static void RemoveShaderResources(dmResource::HFactory factory)
    {
        // dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/color_ramp.vpc");
        // dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/color_ramp.fpc");
        // dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/tessellate.vpc");
        // dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/tessellate.fpc");
        // dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/draw_path.vpc");
        // dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/draw_path.fpc");
        // dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/draw_interior_triangles.vpc");
        // dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/draw_interior_triangles.fpc");
        // dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/draw_image_mesh.vpc");
        // dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/draw_image_mesh.fpc");
    }

    dmResource::Result LoadShaders(dmResource::HFactory factory, ShaderResources** resources)
    {
        /*
        ShaderResources* shaders = new ShaderResources;

        dmResource::Result result = dmResource::RESULT_OK;

        AddShaderResources(factory);

        #define GET_SHADER(path, storage) \
            result = dmResource::Get(factory, path, (void**) &storage); \
            if (result != dmResource::RESULT_OK) \
            { \
                dmLogError("Failed to load resource '%s'", path); \
                return result; \
            }

        GET_SHADER("/defold-rive/assets/pls-shaders/color_ramp.vpc",              shaders->m_RampVs);
        GET_SHADER("/defold-rive/assets/pls-shaders/color_ramp.fpc",              shaders->m_RampFs);
        GET_SHADER("/defold-rive/assets/pls-shaders/tessellate.vpc",              shaders->m_TessVs);
        GET_SHADER("/defold-rive/assets/pls-shaders/tessellate.fpc",              shaders->m_TessFs);
        GET_SHADER("/defold-rive/assets/pls-shaders/draw_path.vpc",               shaders->m_DrawPathVs);
        GET_SHADER("/defold-rive/assets/pls-shaders/draw_path.fpc",               shaders->m_DrawPathFs);
        GET_SHADER("/defold-rive/assets/pls-shaders/draw_interior_triangles.vpc", shaders->m_DrawInteriorTrianglesVs);
        GET_SHADER("/defold-rive/assets/pls-shaders/draw_interior_triangles.fpc", shaders->m_DrawInteriorTrianglesFs);
        GET_SHADER("/defold-rive/assets/pls-shaders/draw_image_mesh.vpc",         shaders->m_DrawImageMeshVs);
        GET_SHADER("/defold-rive/assets/pls-shaders/draw_image_mesh.fpc",         shaders->m_DrawImageMeshFs);

        #undef GET_SHADER

        RemoveShaderResources(factory);

        assert(*resources == 0x0);
        *resources = shaders;
        */

        return dmResource::RESULT_OK;
    }

    static void ReleaseShadersInternal(dmResource::HFactory factory, ShaderResources* shaders)
    {
        /*
        #define RELEASE_SHADER(res) \
            if (res) dmResource::Release(factory, (void*) res);

        RELEASE_SHADER(shaders->m_RampVs);
        RELEASE_SHADER(shaders->m_RampFs);
        RELEASE_SHADER(shaders->m_TessVs);
        RELEASE_SHADER(shaders->m_TessFs);
        RELEASE_SHADER(shaders->m_DrawPathVs);
        RELEASE_SHADER(shaders->m_DrawPathFs);
        RELEASE_SHADER(shaders->m_DrawInteriorTrianglesVs);
        RELEASE_SHADER(shaders->m_DrawInteriorTrianglesFs);
        RELEASE_SHADER(shaders->m_DrawImageMeshVs);
        RELEASE_SHADER(shaders->m_DrawImageMeshFs);

        #undef RELEASE_SHADER
        */
    }

    void ReleaseShaders(dmResource::HFactory factory, ShaderResources** resources)
    {
        /*
        ShaderResources* shaders = *resources;
        ReleaseShadersInternal(factory, shaders);
        delete shaders;
        *resources = 0;
        */
    }

    void DeleteRenderContext(HRenderContext context)
    {
        if (g_RiveRenderer)
        {
            //ReleaseShadersInternal(g_RiveRenderer->m_Factory, &g_RiveRenderer->m_Shaders);
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

            if (width != renderer->m_LastWidth || height != renderer->m_LastHeight)
            {
                renderer->m_RenderContext->OnSizeChanged(width, height);
                renderer->m_LastWidth  = width;
                renderer->m_LastHeight = height;
            }

        #if defined(DM_PLATFORM_MACOS) || defined(DM_PLATFORM_IOS)
            dmGraphics::HTexture swap_chain_texture = dmGraphics::VulkanGetActiveSwapChainTexture(renderer->m_GraphicsContext);
            renderer->m_RenderContext->SetRenderTargetTexture(swap_chain_texture);
        #endif

            int32_t msaa_samples = 0;

        // #if defined(DM_PLATFORM_HTML5)
        //     msaa_samples = 4;
        // #endif

            renderer->m_RenderContext->BeginFrame({
                .renderTargetWidth      = width,
                .renderTargetHeight     = height,
                .clearColor             = 0xff404040,
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
