
#ifdef RIVE_WEBGPU

#include "renderer_context.h"

#include <rive/renderer/rive_renderer.hpp>
#include <rive/renderer/texture.hpp>
#include <rive/renderer/webgpu/render_context_webgpu_impl.hpp>

#include <dmsdk/graphics/graphics_webgpu.h>
#include <dmsdk/dlib/log.h>

#include <defold/defold_graphics.h>

#include <webgpu/webgpu_cpp.h>

namespace dmRive
{
    class DefoldRiveRendererWebGPU : public IDefoldRiveRenderer
    {
    public:
        DefoldRiveRendererWebGPU()
        {
            dmGraphics::HContext graphics_context = dmGraphics::GetInstalledContext();
            assert(graphics_context);

            WGPUDevice webgpu_device = dmGraphics::WebGPUGetDevice(graphics_context);
            WGPUQueue webgpu_queue = dmGraphics::WebGPUGetQueue(graphics_context);

            m_Device = wgpu::Device::Acquire(webgpu_device);
            m_Queue = wgpu::Queue::Acquire(webgpu_queue);

            rive::gpu::RenderContextWebGPUImpl::ContextOptions contextOptions = {
                .plsType = rive::gpu::RenderContextWebGPUImpl::PixelLocalStorageType::EXT_shader_pixel_local_storage,
                .disableStorageBuffers = true,
                .invertRenderTargetY = true,
                // .invertRenderTargetFrontFace = true,
            };

            dmLogInfo("Before creating WebGPU context.");

            m_RenderContext = rive::gpu::RenderContextWebGPUImpl::MakeContext(m_Device, m_Queue, contextOptions);

            dmLogInfo("After creating WebGPU context.");
        }

        rive::Factory* Factory() override
        {
            return m_RenderContext.get();
        }

        rive::Renderer* MakeRenderer() override
        {
            return new rive::RiveRenderer(m_RenderContext.get());
        }

        void BeginFrame(const rive::gpu::RenderContext::FrameDescriptor& frameDescriptor) override
        {
            m_RenderContext->beginFrame(frameDescriptor);

            /*
            s_renderContext->beginFrame({
                .renderTargetWidth = s_renderTarget->width(),
                .renderTargetHeight = s_renderTarget->height(),
                .loadAction = static_cast<gpu::LoadAction>(loadAction),
                .clearColor = clearColor,
            });
            */
        }

        void Flush() override
        {
            m_RenderContext->flush({.renderTarget = m_RenderTarget.get()});
        }

        void OnSizeChanged(uint32_t width, uint32_t height, uint32_t sample_count, bool do_final_blit) override
        {
            dmLogInfo("Before creating RT");
            auto renderContextImpl = m_RenderContext->static_impl_cast<rive::gpu::RenderContextWebGPUImpl>();
            m_RenderTarget         = renderContextImpl->makeRenderTarget(wgpu::TextureFormat::BGRA8Unorm, width, height);
            dmLogInfo("After creating RT");

            if (m_BackingTexture)
            {
                dmGraphics::DeleteTexture(m_BackingTexture);
            }

            dmGraphics::TextureCreationParams default_texture_creation_params;
            default_texture_creation_params.m_Width          = width;
            default_texture_creation_params.m_Height         = height;
            default_texture_creation_params.m_Depth          = 1;
            default_texture_creation_params.m_UsageHintBits  = dmGraphics::TEXTURE_USAGE_FLAG_SAMPLE | dmGraphics::TEXTURE_USAGE_FLAG_COLOR;
            default_texture_creation_params.m_OriginalWidth  = default_texture_creation_params.m_Width;
            default_texture_creation_params.m_OriginalHeight = default_texture_creation_params.m_Height;

            m_BackingTexture = dmGraphics::NewTexture(m_GraphicsContext, default_texture_creation_params);

            dmGraphics::TextureParams tp = {};
            tp.m_Width                   = width;
            tp.m_Height                  = height;
            //tp.m_Format                  = dmGraphics::TEXTURE_FORMAT_BGRA8U;
            tp.m_Format                  = dmGraphics::TEXTURE_FORMAT_RGBA;

            dmGraphics::SetTexture(m_BackingTexture, tp);

            WGPUTextureView webgpu_texture_view = dmGraphics::WebGPUGetTextureView(m_GraphicsContext, m_BackingTexture);
            m_BackingTextureView                = wgpu::TextureView::Acquire(webgpu_texture_view);
            m_RenderTarget->setTargetTextureView(m_BackingTextureView);
        }

        void SetGraphicsContext(dmGraphics::HContext graphics_context) override
        {
            m_GraphicsContext = graphics_context;
        }

        void SetRenderTargetTexture(dmGraphics::HTexture texture) override
        {
            m_TargetTexture = texture;
        }

        dmGraphics::HTexture GetBackingTexture() override
        {
            return m_BackingTexture;
        }

        rive::rcp<rive::gpu::Texture> MakeImageTexture(uint32_t width,
                                                      uint32_t height,
                                                      uint32_t mipLevelCount,
                                                      const uint8_t imageDataRGBA[]) override
        {
            // TODO
            return nullptr;
        }

    private:
        std::unique_ptr<rive::gpu::RenderContext> m_RenderContext;
        rive::rcp<rive::gpu::RenderTargetWebGPU>  m_RenderTarget;
        dmGraphics::HContext                      m_GraphicsContext;
        dmGraphics::HTexture                      m_TargetTexture;
        dmGraphics::HTexture                      m_BackingTexture;

        WGPUDevice                                m_BackendDevice;
        wgpu::Device                              m_Device;
        wgpu::Queue                               m_Queue;
        wgpu::TextureView                         m_BackingTextureView;
    };

    IDefoldRiveRenderer* MakeDefoldRiveRendererWebGPU()
    {
        return new DefoldRiveRendererWebGPU();
    }
}

#endif // RIVE_WEBGPU
