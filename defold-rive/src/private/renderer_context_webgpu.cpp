
#ifdef RIVE_WEBGPU

#include "renderer_context.h"

#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>

#include <rive/renderer/rive_renderer.hpp>
#include <rive/renderer/texture.hpp>
#include <rive/renderer/webgpu/render_context_webgpu_impl.hpp>

#if defined(RIVE_WAGYU) && !defined(DM_GRAPHICS_WEBGPU_WAGYU)
    #define DM_GRAPHICS_WEBGPU_WAGYU
#endif

#include <dmsdk/graphics/graphics_webgpu.h>
#include <dmsdk/graphics/graphics.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/static_assert.h>

#include <webgpu/webgpu_cpp.h>

namespace dmRive
{
    class DefoldRiveRendererWebGPU : public IDefoldRiveRenderer
    {
    public:
        DefoldRiveRendererWebGPU()
        {
#if defined(DM_GRAPHICS_WEBGPU_WAGYU)
            // Making sure we're keeping track of the webgpu.h verssions
            DM_STATIC_ASSERT(WGPUTextureFormat_RG16Snorm == 0x12, Invalid_webgpu_header);
#endif
            dmGraphics::HContext graphics_context = dmGraphics::GetInstalledContext();
            assert(graphics_context);

            WGPUDevice  webgpu_device   = dmGraphics::WebGPUGetDevice(graphics_context);
            WGPUQueue   webgpu_queue    = dmGraphics::WebGPUGetQueue(graphics_context);
            WGPUAdapter webgpu_adapter  = dmGraphics::WebGPUGetAdapter(graphics_context);

            m_RenderToTexture = true; // render to texture is the default
            m_TargetTexture = 0;
            m_BackingTexture = 0;
            m_Adapter = wgpu::Adapter(webgpu_adapter);
            m_Device = wgpu::Device(webgpu_device);
            m_Queue = wgpu::Queue(webgpu_queue);

            dmLogInfo("Before creating WebGPU context");

            rive::gpu::RenderContextWebGPUImpl::ContextOptions contextOptions;
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

            if (!m_RenderToTexture)
            {
                // The texture view gets created every render start, so we need to get it here and set it to the render target
                dmGraphics::HTexture frame_buffer        = dmGraphics::WebGPUGetActiveSwapChainTexture(m_GraphicsContext);
                WGPUTexture          webgpu_texture      = dmGraphics::WebGPUGetTexture(m_GraphicsContext, frame_buffer);
                WGPUTextureView      webgpu_texture_view = dmGraphics::WebGPUGetTextureView(m_GraphicsContext, frame_buffer);
                m_BackingTextureView                     = wgpu::TextureView(webgpu_texture_view);

                m_RenderTarget->setTargetTextureView(m_BackingTextureView, wgpu::Texture(webgpu_texture));
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

                WGPUTexture     webgpu_texture      = dmGraphics::WebGPUGetTexture(m_GraphicsContext, m_BackingTexture);
                WGPUTextureView webgpu_texture_view = dmGraphics::WebGPUGetTextureView(m_GraphicsContext, m_BackingTexture);
                m_BackingTextureView                = wgpu::TextureView(webgpu_texture_view);

                m_RenderTarget->setTargetTextureView(m_BackingTextureView, wgpu::Texture(webgpu_texture));

                m_RenderToTexture = true;
            }
            else
            {
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

        static wgpu::TextureFormat GetASTCFormat(uint8_t blockW, uint8_t blockH)
        {
            // ASTC block sizes mapped to WebGPU sRGB formats (correct for color textures).
            // NOTE: Linear data (e.g. normal maps) would need Unorm variants instead,
            // but ASTC files don't contain color space info. For now we assume sRGB.
            if (blockW == 4 && blockH == 4)   return wgpu::TextureFormat::ASTC4x4UnormSrgb;
            if (blockW == 5 && blockH == 4)   return wgpu::TextureFormat::ASTC5x4UnormSrgb;
            if (blockW == 5 && blockH == 5)   return wgpu::TextureFormat::ASTC5x5UnormSrgb;
            if (blockW == 6 && blockH == 5)   return wgpu::TextureFormat::ASTC6x5UnormSrgb;
            if (blockW == 6 && blockH == 6)   return wgpu::TextureFormat::ASTC6x6UnormSrgb;
            if (blockW == 8 && blockH == 5)   return wgpu::TextureFormat::ASTC8x5UnormSrgb;
            if (blockW == 8 && blockH == 6)   return wgpu::TextureFormat::ASTC8x6UnormSrgb;
            if (blockW == 8 && blockH == 8)   return wgpu::TextureFormat::ASTC8x8UnormSrgb;
            if (blockW == 10 && blockH == 5)  return wgpu::TextureFormat::ASTC10x5UnormSrgb;
            if (blockW == 10 && blockH == 6)  return wgpu::TextureFormat::ASTC10x6UnormSrgb;
            if (blockW == 10 && blockH == 8)  return wgpu::TextureFormat::ASTC10x8UnormSrgb;
            if (blockW == 10 && blockH == 10) return wgpu::TextureFormat::ASTC10x10UnormSrgb;
            if (blockW == 12 && blockH == 10) return wgpu::TextureFormat::ASTC12x10UnormSrgb;
            if (blockW == 12 && blockH == 12) return wgpu::TextureFormat::ASTC12x12UnormSrgb;
            return wgpu::TextureFormat::Undefined;
        }

        rive::rcp<rive::gpu::Texture> MakeImageTextureASTC(uint32_t width,
                                                          uint32_t height,
                                                          uint8_t blockW,
                                                          uint8_t blockH,
                                                          const uint8_t astcData[],
                                                          uint32_t astcDataSize) override
        {
            wgpu::TextureFormat format = GetASTCFormat(blockW, blockH);
            if (format == wgpu::TextureFormat::Undefined)
            {
                dmLogError("Unsupported ASTC block size %dx%d", blockW, blockH);
                return nullptr;
            }

            // All ASTC blocks are 16 bytes regardless of block dimensions
            uint32_t blocksX = (width + blockW - 1) / blockW;
            uint32_t blocksY = (height + blockH - 1) / blockH;
            uint32_t bytesPerRow = blocksX * 16;
            uint32_t expectedSize = blocksX * blocksY * 16;

            if (astcDataSize < expectedSize)
            {
                dmLogError("ASTC data size %u is less than expected %u for %ux%u texture with %dx%d blocks",
                           astcDataSize, expectedSize, width, height, blockW, blockH);
                return nullptr;
            }

            wgpu::TextureDescriptor textureDesc = {
                .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
                .dimension = wgpu::TextureDimension::e2D,
                .size = {width, height},
                .format = format,
                .mipLevelCount = 1,
            };

            wgpu::Texture texture = m_Device.CreateTexture(&textureDesc);

            wgpu::TexelCopyTextureInfo dest = {.texture = texture};
            wgpu::TexelCopyBufferLayout layout = {.bytesPerRow = bytesPerRow};
            wgpu::Extent3D extent = {width, height};

            m_Queue.WriteTexture(&dest,
                                 astcData,
                                 astcDataSize,
                                 &layout,
                                 &extent);

            return rive::make_rcp<rive::gpu::TextureWebGPUImpl>(width, height, std::move(texture));
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
