
#ifdef RIVE_WEBGPU

#include "renderer_context.h"

#if RIVE_WEBGPU == 1
    #include <rive/renderer/webgpu/wagyu-port/old/include/webgpu/webgpu.h>
    #include <rive/renderer/webgpu/wagyu-port/old/include/webgpu/webgpu_cpp.h>
#elif RIVE_WEBGPU == 2
    #include <rive/renderer/webgpu/wagyu-port/new/include/webgpu/webgpu.h>
    #include <rive/renderer/webgpu/wagyu-port/new/include/webgpu/webgpu_cpp.h>
#else
    #error "Unsupported value for RIVE_WEBGPU!"
#endif

#include <rive/renderer/rive_renderer.hpp>
#include <rive/renderer/texture.hpp>
#include <rive/renderer/webgpu/render_context_webgpu_impl.hpp>

#include <dmsdk/graphics/graphics_webgpu.h>
#include <dmsdk/graphics/graphics.h>
#include <dmsdk/dlib/log.h>

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

            WGPUDevice  webgpu_device   = dmGraphics::WebGPUGetDevice(graphics_context);
            WGPUQueue   webgpu_queue    = dmGraphics::WebGPUGetQueue(graphics_context);
            WGPUAdapter webgpu_adapter  = dmGraphics::WebGPUGetAdapter(graphics_context);

            m_RenderToTexture = true; // render to texture is the default
            m_TargetTexture = 0;
            m_BackingTexture = 0;
            m_Adapter = wgpu::Adapter::Acquire(webgpu_adapter);
            m_Device = wgpu::Device::Acquire(webgpu_device);
            m_Queue = wgpu::Queue::Acquire(webgpu_queue);

            rive::gpu::RenderContextWebGPUImpl::ContextOptions contextOptions;
            contextOptions.invertRenderTargetY = true;

            dmLogInfo("Before creating WebGPU context. (RIVE_WEBGPU=%d)", RIVE_WEBGPU);

            m_RenderContext = rive::gpu::RenderContextWebGPUImpl::MakeContext(m_Adapter, m_Device, m_Queue, contextOptions);

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
            // Using operator= (in case it gets implemented)
            rive::gpu::RenderContext::FrameDescriptor copy = frameDescriptor;

            // rendering to a render target
            if (!m_RenderToTexture)
            {
                copy.loadAction = rive::gpu::LoadAction::preserveRenderTarget;
            }

            m_RenderContext->beginFrame(copy);
        }

        void Flush() override
        {
            WGPUCommandEncoder wgpu_encoder;

            if (!m_RenderToTexture)
            {
                wgpu_encoder = dmGraphics::WebGPUGetActiveCommandEncoder(m_GraphicsContext);
                dmGraphics::WebGPURenderPassEnd(m_GraphicsContext);
            }
            else
            {
                wgpu_encoder = wgpuDeviceCreateCommandEncoder(m_Device.Get(), 0);
            }

            m_RenderContext->flush({
                .renderTarget = m_RenderTarget.get(),
                .externalCommandBuffer = (void*)(uintptr_t)wgpu_encoder
            });

            if (!m_RenderToTexture)
            {
                dmGraphics::WebGPURenderPassBegin(m_GraphicsContext);
            }
            else
            {
                const WGPUCommandBuffer buffer = wgpuCommandEncoderFinish(wgpu_encoder, NULL);
                wgpuQueueSubmit(m_Queue.Get(), 1, &buffer);
                wgpuCommandBufferRelease(buffer);
                wgpuCommandEncoderRelease(wgpu_encoder);
            }
        }

        void OnSizeChanged(uint32_t width, uint32_t height, uint32_t sample_count, bool do_final_blit) override
        {
            auto renderContextImpl = m_RenderContext->static_impl_cast<rive::gpu::RenderContextWebGPUImpl>();
            m_RenderTarget         = renderContextImpl->makeRenderTarget(wgpu::TextureFormat::RGBA8Unorm, width, height);

            if (m_BackingTexture)
            {
                dmGraphics::DeleteTexture(m_GraphicsContext, m_BackingTexture);
            }

            if (do_final_blit)
            {
                dmGraphics::TextureCreationParams default_texture_creation_params;
                default_texture_creation_params.m_Width          = width;
                default_texture_creation_params.m_Height         = height;
                default_texture_creation_params.m_Depth          = 1;
                default_texture_creation_params.m_UsageHintBits  = dmGraphics::TEXTURE_USAGE_FLAG_SAMPLE | dmGraphics::TEXTURE_USAGE_FLAG_COLOR | dmGraphics::TEXTURE_USAGE_FLAG_INPUT;
                default_texture_creation_params.m_OriginalWidth  = default_texture_creation_params.m_Width;
                default_texture_creation_params.m_OriginalHeight = default_texture_creation_params.m_Height;

                m_BackingTexture = dmGraphics::NewTexture(m_GraphicsContext, default_texture_creation_params);

                dmGraphics::TextureParams tp = {};
                tp.m_Width                   = width;
                tp.m_Height                  = height;
                //tp.m_Format                  = dmGraphics::TEXTURE_FORMAT_BGRA8U;
                tp.m_Format                  = dmGraphics::TEXTURE_FORMAT_RGBA;

                dmGraphics::SetTexture(m_GraphicsContext, m_BackingTexture, tp);

                WGPUTextureView webgpu_texture_view = dmGraphics::WebGPUGetTextureView(m_GraphicsContext, m_BackingTexture);
                m_BackingTextureView                = wgpu::TextureView::Acquire(webgpu_texture_view);
                m_RenderTarget->setTargetTextureView(m_BackingTextureView);

                m_RenderToTexture = true;
            }
            else
            {
                dmGraphics::HTexture frame_buffer   = dmGraphics::WebGPUGetActiveSwapChainTexture(m_GraphicsContext);
                WGPUTextureView webgpu_texture_view = dmGraphics::WebGPUGetTextureView(m_GraphicsContext, frame_buffer);
                m_BackingTextureView                = wgpu::TextureView::Acquire(webgpu_texture_view);
                m_RenderTarget->setTargetTextureView(m_BackingTextureView);

                m_RenderToTexture = false;
            }
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
            if (mipLevelCount < 1)
                mipLevelCount = 1;

            auto renderContextImpl = m_RenderContext->static_impl_cast<rive::gpu::RenderContextWebGPUImpl>();
            auto texture = renderContextImpl->makeImageTexture(width, height, mipLevelCount, imageDataRGBA);
            return texture;
        }

    private:
        std::unique_ptr<rive::gpu::RenderContext> m_RenderContext;
        rive::rcp<rive::gpu::RenderTargetWebGPU>  m_RenderTarget;
        dmGraphics::HContext                      m_GraphicsContext;
        dmGraphics::HTexture                      m_TargetTexture;
        dmGraphics::HTexture                      m_BackingTexture;

        WGPUDevice                                m_BackendDevice;
        wgpu::Adapter                             m_Adapter;
        wgpu::Device                              m_Device;
        wgpu::Queue                               m_Queue;
        wgpu::TextureView                         m_BackingTextureView;

        bool                                      m_RenderToTexture;
    };

    IDefoldRiveRenderer* MakeDefoldRiveRendererWebGPU()
    {
        return new DefoldRiveRendererWebGPU();
    }
}

#endif // RIVE_WEBGPU
