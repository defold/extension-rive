
#ifdef RIVE_WEBGPU

#include "renderer_context.h"

#include <rive/renderer/rive_renderer.hpp>
#include <rive/renderer/texture.hpp>
#include <rive/renderer/webgpu/render_context_webgpu_impl.hpp>

#include <dmsdk/graphics/graphics_vulkan.h>

#include <defold/defold_graphics.h>

namespace dmRive
{
    class DefoldRiveRendererWebGPU : public IDefoldRiveRenderer
    {
    public:
        DefoldRiveRendererWebGPU()
        {
            // rive::gpu::RenderContextMetalImpl::ContextOptions metalOptions;
            // metalOptions.synchronousShaderCompilations = true;
            // metalOptions.disableFramebufferReads = true;
            // m_RenderContext = rive::gpu::RenderContextMetalImpl::MakeContext(m_GPU, metalOptions);

            m_RenderContext = rive::gpu::RenderContextWebGPUImpl::MakeContext(m_Device, m_Queue, rive::gpu::RenderContextWebGPUImpl::ContextOptions());
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
        }

        void Flush() override
        {
            m_RenderContext->flush({.renderTarget = m_RenderTarget.get()});
        }

        void OnSizeChanged(uint32_t width, uint32_t height, uint32_t sample_count) override
        {
            auto renderContextImpl = m_RenderContext->static_impl_cast<rive::gpu::RenderContextWebGPUImpl>();
            m_RenderTarget         = renderContextImpl->makeRenderTarget(wgpu::TextureFormat::BGRA8Unorm, width, height);
        }

        void SetGraphicsContext(dmGraphics::HContext graphics_context) override
        {
            m_GraphicsContext = graphics_context;

            /*
            // Update command queue
            void* cmd_queue = dmGraphics::VulkanGraphicsCommandQueueToMetal(graphics_context);
            assert(cmd_queue);
            m_Queue = (__bridge id<MTLCommandQueue>) cmd_queue;
            m_BackingTexture = dmGraphics::NewTexture(m_GraphicsContext, {});
            */
        }

        void SetRenderTargetTexture(dmGraphics::HTexture texture) override
        {
            // m_TargetTexture = texture;
        }

        dmGraphics::HTexture GetBackingTexture() override
        {
            return 0; // m_BackingTexture;
        }

        rive::rcp<rive::gpu::Texture> MakeImageTexture(uint32_t width,
                                                      uint32_t height,
                                                      uint32_t mipLevelCount,
                                                      const uint8_t imageDataRGBA[]) override
        {
            /*
            auto renderContextImpl = m_RenderContext->static_impl_cast<rive::gpu::RenderContextMetalImpl>();
            // OpenGL vs Metal difference..
            if (mipLevelCount < 1)
            {
                mipLevelCount = 1;
            }
            return renderContextImpl->makeImageTexture(width, height, mipLevelCount, imageDataRGBA);
            */
            return nullptr;
        }

    private:
        // id<MTLDevice>                             m_GPU   = MTLCreateSystemDefaultDevice();
        // id<MTLCommandQueue>                       m_Queue;
        // std::unique_ptr<rive::gpu::RenderContext> m_RenderContext;
        // rive::rcp<rive::gpu::RenderTargetMetal>   m_RenderTarget;
        // dmGraphics::HContext                      m_GraphicsContext;
        // dmGraphics::HTexture                      m_BackingTexture;
        // dmGraphics::HTexture                      m_TargetTexture;

        std::unique_ptr<rive::gpu::RenderContext> m_RenderContext;
        rive::rcp<rive::gpu::RenderTargetWebGPU>  m_RenderTarget;
        dmGraphics::HContext                      m_GraphicsContext;

        WGPUDevice                                m_BackendDevice;
        wgpu::Device                              m_Device;
        wgpu::Queue                               m_Queue;
    };

    IDefoldRiveRenderer* MakeDefoldRiveRendererWebGPU()
    {
        return new DefoldRiveRendererWebGPU();
    }
}

#endif // RIVE_WEBGPU
