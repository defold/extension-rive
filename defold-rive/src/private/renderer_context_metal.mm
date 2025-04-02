
#include "renderer_context.h"

#include <rive/renderer/rive_renderer.hpp>
#include <rive/renderer/texture.hpp>
#include <rive/renderer/metal/render_context_metal_impl.h>

#include <dmsdk/graphics/graphics_vulkan.h>

#include <defold/defold_graphics.h>

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

        void OnSizeChanged(uint32_t width, uint32_t height, uint32_t sample_count, bool do_final_blit) override
        {
            // For now we can only output the result if we blit it to the backbuffer via an extra DC
            assert(do_final_blit);

            auto renderContextImpl = m_RenderContext->static_impl_cast<rive::gpu::RenderContextMetalImpl>();
            m_RenderTarget         = renderContextImpl->makeRenderTarget(MTLPixelFormatBGRA8Unorm, width, height);

            dmGraphics::TextureParams tp = {};
            tp.m_Width                   = width;
            tp.m_Height                  = height;
            tp.m_Depth                   = 1;
            tp.m_Format                  = dmGraphics::TEXTURE_FORMAT_BGRA8U;

            dmGraphics::SetTexture(m_BackingTexture, tp);

            void* opaque_backing_texture = dmGraphics::VulkanTextureToMetal(m_GraphicsContext, m_BackingTexture);
            assert(opaque_backing_texture);
            id<MTLTexture> mtl_texture = (__bridge id<MTLTexture>) opaque_backing_texture;
            m_RenderTarget->setTargetTexture(mtl_texture);
        }

        void SetGraphicsContext(dmGraphics::HContext graphics_context) override
        {
            m_GraphicsContext = graphics_context;

            // Update command queue
            void* cmd_queue = dmGraphics::VulkanGraphicsCommandQueueToMetal(graphics_context);
            assert(cmd_queue);
            m_Queue = (__bridge id<MTLCommandQueue>) cmd_queue;
            m_BackingTexture = dmGraphics::NewTexture(m_GraphicsContext, {});
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
            auto renderContextImpl = m_RenderContext->static_impl_cast<rive::gpu::RenderContextMetalImpl>();
            // OpenGL vs Metal difference..
            if (mipLevelCount < 1)
            {
                mipLevelCount = 1;
            }
            return renderContextImpl->makeImageTexture(width, height, mipLevelCount, imageDataRGBA);
        }

    private:
        id<MTLDevice>                             m_GPU   = MTLCreateSystemDefaultDevice();
        id<MTLCommandQueue>                       m_Queue;
        std::unique_ptr<rive::gpu::RenderContext> m_RenderContext;
        rive::rcp<rive::gpu::RenderTargetMetal>   m_RenderTarget;
        dmGraphics::HContext                      m_GraphicsContext;
        dmGraphics::HTexture                      m_BackingTexture;
        dmGraphics::HTexture                      m_TargetTexture;
    };

    IDefoldRiveRenderer* MakeDefoldRiveRendererMetal()
    {
        return new DefoldRiveRendererMetal();
    }
}
