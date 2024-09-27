
#include "renderer_context.h"

#include <rive/renderer/rive_renderer.hpp>
#include <rive/renderer/metal/render_context_metal_impl.h>

#include <dmsdk/graphics/graphics_vulkan.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

namespace dmRive
{
    class DefoldRiveRendererMetal : public IDefoldRiveRenderer
    {
    public:
        DefoldRiveRendererMetal()
        {
            rive::gpu::RenderContextMetalImpl::ContextOptions metalOptions;
            metalOptions.synchronousShaderCompilations = true;
            metalOptions.disableFramebufferReads = true;

            m_RenderContext = rive::gpu::RenderContextMetalImpl::MakeContext(m_GPU, metalOptions);
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
            id<MTLCommandBuffer> flushCommandBuffer = [m_Queue commandBuffer];
            m_RenderContext->flush({
                .renderTarget = m_RenderTarget.get(),
                .externalCommandBuffer = (__bridge void*) flushCommandBuffer
            });

            [flushCommandBuffer commit];
        }

        void OnSizeChanged(uint32_t width, uint32_t height) override
        {
            auto renderContextImpl = m_RenderContext->static_impl_cast<rive::gpu::RenderContextMetalImpl>();
            m_RenderTarget         = renderContextImpl->makeRenderTarget(MTLPixelFormatBGRA8Unorm, width, height);
        }

        void SetGraphicsContext(dmGraphics::HContext graphics_context) override
        {
            m_GraphicsContext = graphics_context;

            // Update command queue
            void* cmd_queue = dmGraphics::VulkanGraphicsCommandQueueToMetal(graphics_context);
            assert(cmd_queue);
            m_Queue = (__bridge id<MTLCommandQueue>) cmd_queue;
        }

        void SetRenderTargetTexture(dmGraphics::HTexture texture) override
        {
            void* opaque = dmGraphics::VulkanTextureToMetal(m_GraphicsContext, texture);
            assert(opaque);

            id<MTLTexture> mtl_texture = (__bridge id<MTLTexture>) opaque;
            m_RenderTarget->setTargetTexture(mtl_texture);
        }

    private:
        id<MTLDevice>                             m_GPU   = MTLCreateSystemDefaultDevice();
        id<MTLCommandQueue>                       m_Queue;
        std::unique_ptr<rive::gpu::RenderContext> m_RenderContext;
        rive::rcp<rive::gpu::RenderTargetMetal>   m_RenderTarget;
        dmGraphics::HContext                      m_GraphicsContext;
    };

    IDefoldRiveRenderer* MakeDefoldRiveRendererMetal()
    {
        return new DefoldRiveRendererMetal();
    }
}
