#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/image.h>
#include <dmsdk/graphics/graphics_vulkan.h>

#include <rive/pls/pls_image.hpp>
#include <rive/pls/pls_renderer.hpp>

// PLS Renderer
#include <private/renderer_private.h>

#include <private/shaders/color_ramp.vpc.gen.h>
#include <private/shaders/color_ramp.fpc.gen.h>
#include <private/shaders/draw_interior_triangles.vpc.gen.h>
#include <private/shaders/draw_interior_triangles.fpc.gen.h>
#include <private/shaders/draw_path.vpc.gen.h>
#include <private/shaders/draw_path.fpc.gen.h>
#include <private/shaders/tessellate.vpc.gen.h>
#include <private/shaders/tessellate.fpc.gen.h>
#include <private/shaders/draw_image_mesh.vpc.gen.h>
#include <private/shaders/draw_image_mesh.fpc.gen.h>

#include <common/vertices.h>
#include <renderer.h>

#include <assert.h>

namespace dmResource
{
    void IncRef(HFactory factory, void* resource);
}

namespace dmGraphics
{
    void RepackRGBToRGBA(uint32_t num_pixels, uint8_t* rgb, uint8_t* rgba);
}

namespace dmRender
{
    const dmVMath::Matrix4& GetViewMatrix(HRenderContext render_context);
    const dmVMath::Matrix4& GetViewProjectionMatrix(HRenderContext render_context);
}

namespace dmRive
{
    struct DefoldRiveRenderer
    {
    #if defined(DM_PLATFORM_MACOS) || defined(DM_PLATFORM_IOS)
        std::unique_ptr<rive::pls::PLSRenderContext> m_PLSRenderContext = DefoldPLSRenderContext::MakeContext(CONFIG_SUB_PASS_LOAD);
    #elif defined(DM_PLATFORM_WINDOWS) || defined(DM_PLATFORM_LINUX) || defined(DM_PLATFORM_ANDROID) || defined(DM_PLATFORM_SWITCH)
        std::unique_ptr<rive::pls::PLSRenderContext> m_PLSRenderContext = DefoldPLSRenderContext::MakeContext(CONFIG_RW_TEXTURE);
    #else
        #error "Platform not supported"
        assert(0 && "Platform not supported");
    #endif
        ShaderResources                              m_Shaders;
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
        dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/color_ramp.vpc",              COLOR_RAMP_VPC_SIZE, COLOR_RAMP_VPC);
        dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/color_ramp.fpc",              COLOR_RAMP_FPC_SIZE, COLOR_RAMP_FPC);
        dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/tessellate.vpc",              TESSELLATE_VPC_SIZE, TESSELLATE_VPC);
        dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/tessellate.fpc",              TESSELLATE_FPC_SIZE, TESSELLATE_FPC);
        dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/draw_path.vpc",               DRAW_PATH_VPC_SIZE, DRAW_PATH_VPC);
        dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/draw_path.fpc",               DRAW_PATH_FPC_SIZE, DRAW_PATH_FPC);
        dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/draw_interior_triangles.vpc", DRAW_INTERIOR_TRIANGLES_VPC_SIZE, DRAW_INTERIOR_TRIANGLES_VPC);
        dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/draw_interior_triangles.fpc", DRAW_INTERIOR_TRIANGLES_FPC_SIZE, DRAW_INTERIOR_TRIANGLES_FPC);
        dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/draw_image_mesh.vpc",         DRAW_IMAGE_MESH_VPC_SIZE, DRAW_IMAGE_MESH_VPC);
        dmResource::AddFile(factory, "/defold-rive/assets/pls-shaders/draw_image_mesh.fpc",         DRAW_IMAGE_MESH_FPC_SIZE, DRAW_IMAGE_MESH_FPC);
    }

    static void RemoveShaderResources(dmResource::HFactory factory)
    {
        dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/color_ramp.vpc");
        dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/color_ramp.fpc");
        dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/tessellate.vpc");
        dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/tessellate.fpc");
        dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/draw_path.vpc");
        dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/draw_path.fpc");
        dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/draw_interior_triangles.vpc");
        dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/draw_interior_triangles.fpc");
        dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/draw_image_mesh.vpc");
        dmResource::RemoveFile(factory, "/defold-rive/assets/pls-shaders/draw_image_mesh.fpc");
    }

    dmResource::Result LoadShaders(dmResource::HFactory factory, ShaderResources** resources)
    {
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

        return dmResource::RESULT_OK;
    }

    static void ReleaseShadersInternal(dmResource::HFactory factory, ShaderResources* shaders)
    {
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
    }

    void ReleaseShaders(dmResource::HFactory factory, ShaderResources** resources)
    {
        ShaderResources* shaders = *resources;
        ReleaseShadersInternal(factory, shaders);
        delete shaders;
        *resources = 0;
    }

    void DeleteRenderContext(HRenderContext context)
    {
        if (g_RiveRenderer)
        {
            ReleaseShadersInternal(g_RiveRenderer->m_Factory, &g_RiveRenderer->m_Shaders);
            delete g_RiveRenderer;
            g_RiveRenderer = 0;
        }
    }

    rive::Factory* GetRiveFactory(HRenderContext context)
    {
        DefoldRiveRenderer* renderer = (DefoldRiveRenderer*) context;
        return renderer->m_PLSRenderContext.get();
    }

    rive::Renderer* GetRiveRenderer(HRenderContext context)
    {
        DefoldRiveRenderer* renderer = (DefoldRiveRenderer*) context;
        return renderer->m_RiveRenderer;
    }

    void RenderBegin(HRenderContext context, ShaderResources* shaders, dmResource::HFactory factory)
    {
        DefoldRiveRenderer* renderer = (DefoldRiveRenderer*) context;

        if (!renderer->m_GraphicsContext)
        {
            renderer->m_GraphicsContext = dmGraphics::VulkanGetContext();
            assert(renderer->m_GraphicsContext);
        }

        auto pls_render_context = renderer->m_PLSRenderContext->static_impl_cast<DefoldPLSRenderContext>();

        if (!renderer->m_RiveRenderer)
        {
            renderer->m_RiveRenderer = new rive::pls::PLSRenderer(renderer->m_PLSRenderContext.get());
            pls_render_context->setDefoldContext(renderer->m_GraphicsContext);

            dmResource::IncRef(factory, (void*) shaders->m_RampVs);
            dmResource::IncRef(factory, (void*) shaders->m_RampFs);
            dmResource::IncRef(factory, (void*) shaders->m_TessVs);
            dmResource::IncRef(factory, (void*) shaders->m_TessFs);
            dmResource::IncRef(factory, (void*) shaders->m_DrawPathVs);
            dmResource::IncRef(factory, (void*) shaders->m_DrawPathFs);
            dmResource::IncRef(factory, (void*) shaders->m_DrawInteriorTrianglesVs);
            dmResource::IncRef(factory, (void*) shaders->m_DrawInteriorTrianglesFs);
            dmResource::IncRef(factory, (void*) shaders->m_DrawImageMeshVs);
            dmResource::IncRef(factory, (void*) shaders->m_DrawImageMeshFs);

            renderer->m_Factory = factory;
            renderer->m_Shaders = *shaders;

            dmRive::DefoldShaderList programs = {};
            programs.m_Ramp                   = dmGraphics::NewProgram(renderer->m_GraphicsContext, shaders->m_RampVs, shaders->m_RampFs);
            programs.m_Tess                   = dmGraphics::NewProgram(renderer->m_GraphicsContext, shaders->m_TessVs, shaders->m_TessFs);
            programs.m_DrawPath               = dmGraphics::NewProgram(renderer->m_GraphicsContext, shaders->m_DrawPathVs, shaders->m_DrawPathFs);
            programs.m_DrawInteriorTriangles  = dmGraphics::NewProgram(renderer->m_GraphicsContext, shaders->m_DrawInteriorTrianglesVs, shaders->m_DrawInteriorTrianglesFs);
            programs.m_DrawImageMesh          = dmGraphics::NewProgram(renderer->m_GraphicsContext, shaders->m_DrawImageMeshVs, shaders->m_DrawImageMeshFs);

            pls_render_context->setDefoldShaders(programs);
        }

        if (!renderer->m_FrameBegin)
        {
            // TODO: Figure out why we are getting de-synced frames on osx..
            dmGraphics::VulkanSetFrameInFlightCount(renderer->m_GraphicsContext, 1);

            uint32_t width  = dmGraphics::GetWindowWidth(renderer->m_GraphicsContext);
            uint32_t height = dmGraphics::GetWindowHeight(renderer->m_GraphicsContext);
            if (width != renderer->m_LastWidth || height != renderer->m_LastHeight)
            {
                pls_render_context->onSizeChanged(width, height);
                renderer->m_LastWidth  = width;
                renderer->m_LastHeight = height;
            }

            dmRive::DefoldPLSRenderTarget* rt = static_cast<dmRive::DefoldPLSRenderTarget*>(pls_render_context->renderTarget().get());

            rt->setTargetTexture(dmGraphics::VulkanGetActiveSwapChainTexture(renderer->m_GraphicsContext));

            rive::pls::PLSRenderContext::FrameDescriptor frameDescriptor = {};
            //frameDescriptor.renderTarget                                 = pls_render_context->renderTarget();
            frameDescriptor.clearColor         = 0;
            frameDescriptor.renderTargetWidth  = width;
            frameDescriptor.renderTargetHeight = height;

            renderer->m_PLSRenderContext->beginFrame(std::move(frameDescriptor));
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
            auto pls_render_context = renderer->m_PLSRenderContext->static_impl_cast<DefoldPLSRenderContext>();

            rive::pls::PLSRenderContext::FlushResources flushResources = {};
            flushResources.renderTarget = pls_render_context->renderTarget().get();

            renderer->m_PLSRenderContext->flush(flushResources);
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
        auto pls_render_context      = renderer->m_PLSRenderContext->static_impl_cast<DefoldPLSRenderContext>();

        rive::rcp<rive::pls::PLSTexture> pls_texture;
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

            pls_texture = pls_render_context->makeImageTexture(img_width, img_height, 1, (const uint8_t*) bitmap_data_rgba);

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
            pls_texture = pls_render_context->makeImageTexture(1, 1, 1, (const uint8_t*) pink);
        }

        return pls_texture != nullptr ? rive::make_rcp<rive::pls::PLSImage>(std::move(pls_texture)) : nullptr;
    }

    rive::Mat2D GetViewTransform(HRenderContext context, dmRender::HRenderContext render_context)
    {
        const dmVMath::Matrix4& view_matrix = dmRender::GetViewMatrix(render_context);
        rive::Mat2D viewTransform;
        Mat4ToMat2D(view_matrix, viewTransform);
        return viewTransform;
    }

    rive::Mat2D GetViewProjTransform(HRenderContext context, dmRender::HRenderContext render_context)
    {
        const dmVMath::Matrix4& view_proj_matrix = dmRender::GetViewProjectionMatrix(render_context);

        dmVMath::Matrix4 ndc_matrix = dmVMath::Matrix4::identity();
        ndc_matrix.setElem(2, 2, 0.5f );
        ndc_matrix.setElem(3, 2, 0.5f );
        dmVMath::Matrix4 vulkan_view_projection = ndc_matrix * view_proj_matrix;

        vulkan_view_projection[3][0] = view_proj_matrix[3][0];
        vulkan_view_projection[3][1] = view_proj_matrix[3][1];

        rive::Mat2D rive_transform;
        Mat4ToMat2D(vulkan_view_projection, rive_transform);
        return rive_transform;
    }
}
