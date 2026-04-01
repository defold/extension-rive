#if defined(DM_GRAPHICS_USE_VULKAN) && defined(RIVE_VULKAN) && !defined(DM_HEADLESS)

#include "renderer_context.h"

#include <dmsdk/dlib/log.h>
#include <dmsdk/graphics/graphics_vulkan.h>
#include <dmsdk/graphics/graphics.h>

#include <rive/renderer/rive_renderer.hpp>
#include <rive/renderer/texture.hpp>
#include <rive/renderer/vulkan/render_context_vulkan_impl.hpp>
#include <rive/renderer/vulkan/render_target_vulkan.hpp>
#include <rive/renderer/vulkan/vkutil.hpp>

namespace dmRive
{
    class DefoldRiveRendererVulkan : public IDefoldRiveRenderer
    {
    public:
        DefoldRiveRendererVulkan()
        {
            m_GraphicsContext = 0;
            m_BackingRenderTarget = 0;
            m_BackingTexture = 0;
            m_TargetTexture = 0;
            m_Device = VK_NULL_HANDLE;
            m_GraphicsQueue = VK_NULL_HANDLE;
            m_GraphicsQueueFamily = 0;
            m_CommandPool = VK_NULL_HANDLE;
            m_FlushCommandBuffer = VK_NULL_HANDLE;
            m_FlushFence = VK_NULL_HANDLE;
            m_FrameNumber = 0;
            m_Width = 0;
            m_Height = 0;
            m_DoFinalBlit = true;
        }

        ~DefoldRiveRendererVulkan() override
        {
            DestroySubmitResources();

            if (m_BackingRenderTarget)
            {
                dmGraphics::DeleteRenderTarget(m_GraphicsContext, m_BackingRenderTarget);
                m_BackingRenderTarget = 0;
                m_BackingTexture = 0;
            }
        }

        rive::Factory* Factory() override
        {
            EnsureRenderContext();
            return m_RenderContext.get();
        }

        rive::Renderer* MakeRenderer() override
        {
            EnsureRenderContext();
            if (!m_RenderContext)
            {
                return 0;
            }
            return new rive::RiveRenderer(m_RenderContext.get());
        }

        void BeginFrame(const rive::gpu::RenderContext::FrameDescriptor& frameDescriptor) override
        {
            if (!m_RenderContext)
            {
                return;
            }

            m_RenderContext->beginFrame(frameDescriptor);
        }

        void Flush() override
        {
            if (!m_RenderContext || !m_RenderTarget || !m_GraphicsContext)
            {
                return;
            }

            if (!EnsureSubmitResources())
            {
                return;
            }

            VkResult vk_result = vkWaitForFences(m_Device, 1, &m_FlushFence, VK_TRUE, UINT64_MAX);
            if (vk_result != VK_SUCCESS)
            {
                dmLogError("vkWaitForFences failed: %d", (int)vk_result);
                return;
            }

            vk_result = vkResetFences(m_Device, 1, &m_FlushFence);
            if (vk_result != VK_SUCCESS)
            {
                dmLogError("vkResetFences failed: %d", (int)vk_result);
                return;
            }

            vk_result = vkResetCommandPool(m_Device, m_CommandPool, 0);
            if (vk_result != VK_SUCCESS)
            {
                dmLogError("vkResetCommandPool failed: %d", (int)vk_result);
                return;
            }

            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vk_result = vkBeginCommandBuffer(m_FlushCommandBuffer, &begin_info);
            if (vk_result != VK_SUCCESS)
            {
                dmLogError("vkBeginCommandBuffer failed: %d", (int)vk_result);
                return;
            }

            rive::gpu::RenderContext::FlushResources flush_resources;
            flush_resources.renderTarget = m_RenderTarget.get();
            flush_resources.externalCommandBuffer = (void*)m_FlushCommandBuffer;
            flush_resources.currentFrameNumber = ++m_FrameNumber;
            // Defold does not expose Vulkan fence-safe frame numbers through the
            // public SDK, so keep a conservative two-frame delay before Rive
            // recycles transient GPU resources.
            flush_resources.safeFrameNumber = (m_FrameNumber > 2) ? (m_FrameNumber - 2) : 0;
            m_RenderContext->flush(flush_resources);

            if (m_DoFinalBlit)
            {
                const rive::gpu::vkutil::ImageAccess shader_read_access = {
                    .pipelineStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    .accessMask = VK_ACCESS_SHADER_READ_BIT,
                    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };
                m_RenderTarget->accessTargetImageView(m_FlushCommandBuffer, shader_read_access);
            }
            else
            {
                const rive::gpu::vkutil::ImageAccess present_access = {
                    .pipelineStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    .accessMask = 0,
                    .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                };
                m_RenderTarget->accessTargetImageView(m_FlushCommandBuffer, present_access);
            }

            vk_result = vkEndCommandBuffer(m_FlushCommandBuffer);
            if (vk_result != VK_SUCCESS)
            {
                dmLogError("vkEndCommandBuffer failed: %d", (int)vk_result);
                return;
            }

            VkSubmitInfo submit_info = {};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &m_FlushCommandBuffer;
            vk_result = vkQueueSubmit(m_GraphicsQueue, 1, &submit_info, m_FlushFence);
            if (vk_result != VK_SUCCESS)
            {
                dmLogError("vkQueueSubmit failed: %d", (int)vk_result);
            }
        }

        void OnSizeChanged(uint32_t width, uint32_t height, uint32_t sample_count, bool do_final_blit) override
        {
            (void)sample_count;

            EnsureRenderContext();
            if (!m_RenderContext)
            {
                return;
            }

            m_Width = width;
            m_Height = height;
            m_DoFinalBlit = do_final_blit;

            auto render_context_impl = m_RenderContext->static_impl_cast<rive::gpu::RenderContextVulkanImpl>();
            m_RenderTarget = render_context_impl->makeRenderTarget(width,
                                                                   height,
                                                                   VK_FORMAT_B8G8R8A8_UNORM,
                                                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                                   VK_IMAGE_USAGE_SAMPLED_BIT |
                                                                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                                   (do_final_blit ? VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT : 0));

            if (!m_RenderTarget)
            {
                dmLogError("Failed to create Rive Vulkan render target");
                return;
            }

            if (do_final_blit)
            {
                if (!EnsureBackingRenderTarget(width, height))
                {
                    dmLogError("Failed to create Vulkan backing render target");
                    return;
                }
                BindTargetTexture(m_BackingTexture);
            }
            else if (m_TargetTexture != 0)
            {
                BindTargetTexture(m_TargetTexture);
            }
        }

        void SetGraphicsContext(dmGraphics::HContext graphics_context) override
        {
            m_GraphicsContext = graphics_context;
            EnsureRenderContext();
        }

        void SetRenderTargetTexture(dmGraphics::HTexture texture) override
        {
            m_TargetTexture = texture;
            if (!m_DoFinalBlit && m_RenderTarget && texture != 0)
            {
                BindTargetTexture(texture);
            }
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
            if (!m_RenderContext)
            {
                return nullptr;
            }

            if (mipLevelCount < 1)
            {
                mipLevelCount = 1;
            }

            auto render_context_impl = m_RenderContext->static_impl_cast<rive::gpu::RenderContextVulkanImpl>();
            return render_context_impl->makeImageTexture(width, height, mipLevelCount, imageDataRGBA);
        }

        rive::rcp<rive::gpu::Texture> MakeImageTextureASTC(uint32_t width,
                                                          uint32_t height,
                                                          uint8_t blockW,
                                                          uint8_t blockH,
                                                          const uint8_t astcData[],
                                                          uint32_t astcDataSize) override
        {
            (void)width;
            (void)height;
            (void)blockW;
            (void)blockH;
            (void)astcData;
            (void)astcDataSize;
            dmLogWarning("MakeImageTextureASTC not implemented for Vulkan");
            return nullptr;
        }

    private:
        bool EnsureRenderContext()
        {
            if (!m_GraphicsContext)
            {
                return false;
            }

            VkInstance instance = dmGraphics::VulkanGetInstance(m_GraphicsContext);
            VkPhysicalDevice physical_device = dmGraphics::VulkanGetPhysicalDevice(m_GraphicsContext);
            VkDevice device = dmGraphics::VulkanGetDevice(m_GraphicsContext);
            VkQueue graphics_queue = dmGraphics::VulkanGetGraphicsQueue(m_GraphicsContext);
            uint32_t graphics_queue_family = (uint32_t)dmGraphics::VulkanGetGraphicsQueueFamily(m_GraphicsContext);

            if (instance == VK_NULL_HANDLE || physical_device == VK_NULL_HANDLE || device == VK_NULL_HANDLE || graphics_queue == VK_NULL_HANDLE)
            {
                dmLogError("Vulkan graphics context missing required handles");
                return false;
            }

            m_Device = device;
            m_GraphicsQueue = graphics_queue;
            m_GraphicsQueueFamily = graphics_queue_family;

            if (m_RenderContext)
            {
                return true;
            }

            rive::gpu::VulkanFeatures vulkan_features = {};
            VkPhysicalDeviceFeatures physical_features = {};
            vkGetPhysicalDeviceFeatures(physical_device, &physical_features);
            vulkan_features.independentBlend = physical_features.independentBlend == VK_TRUE;
            vulkan_features.fillModeNonSolid = physical_features.fillModeNonSolid == VK_TRUE;
            vulkan_features.fragmentStoresAndAtomics = physical_features.fragmentStoresAndAtomics == VK_TRUE;
            vulkan_features.shaderClipDistance = physical_features.shaderClipDistance == VK_TRUE;

            VkPhysicalDeviceProperties properties = {};
            vkGetPhysicalDeviceProperties(physical_device, &properties);
            vulkan_features.apiVersion = properties.apiVersion;

            vulkan_features.rasterizationOrderColorAttachmentAccess =
                dmGraphics::IsExtensionSupported(m_GraphicsContext, "VK_EXT_rasterization_order_attachment_access");
            vulkan_features.fragmentShaderPixelInterlock =
                dmGraphics::IsExtensionSupported(m_GraphicsContext, "VK_EXT_fragment_shader_interlock");
            vulkan_features.VK_KHR_portability_subset =
                dmGraphics::IsExtensionSupported(m_GraphicsContext, "VK_KHR_portability_subset");

            rive::gpu::RenderContextVulkanImpl::ContextOptions options;
            options.shaderCompilationMode = rive::gpu::ShaderCompilationMode::standard;

            m_RenderContext = rive::gpu::RenderContextVulkanImpl::MakeContext(instance,
                                                                              physical_device,
                                                                              device,
                                                                              vulkan_features,
                                                                              vkGetInstanceProcAddr,
                                                                              options);
            if (!m_RenderContext)
            {
                dmLogError("Failed to create Rive Vulkan render context");
                return false;
            }

            return true;
        }

        bool EnsureSubmitResources()
        {
            if (!EnsureRenderContext())
            {
                return false;
            }

            if (m_CommandPool != VK_NULL_HANDLE && m_FlushCommandBuffer != VK_NULL_HANDLE && m_FlushFence != VK_NULL_HANDLE)
            {
                return true;
            }

            VkCommandPoolCreateInfo pool_info = {};
            pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            pool_info.queueFamilyIndex = m_GraphicsQueueFamily;
            VkResult vk_result = vkCreateCommandPool(m_Device, &pool_info, 0, &m_CommandPool);
            if (vk_result != VK_SUCCESS)
            {
                dmLogError("vkCreateCommandPool failed: %d", (int)vk_result);
                DestroySubmitResources();
                return false;
            }

            VkCommandBufferAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            alloc_info.commandPool = m_CommandPool;
            alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            alloc_info.commandBufferCount = 1;
            vk_result = vkAllocateCommandBuffers(m_Device, &alloc_info, &m_FlushCommandBuffer);
            if (vk_result != VK_SUCCESS)
            {
                dmLogError("vkAllocateCommandBuffers failed: %d", (int)vk_result);
                DestroySubmitResources();
                return false;
            }

            VkFenceCreateInfo fence_info = {};
            fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            vk_result = vkCreateFence(m_Device, &fence_info, 0, &m_FlushFence);
            if (vk_result != VK_SUCCESS)
            {
                dmLogError("vkCreateFence failed: %d", (int)vk_result);
                DestroySubmitResources();
                return false;
            }

            return true;
        }

        void DestroySubmitResources()
        {
            if (m_Device == VK_NULL_HANDLE)
            {
                return;
            }

            if (m_FlushFence != VK_NULL_HANDLE)
            {
                vkWaitForFences(m_Device, 1, &m_FlushFence, VK_TRUE, UINT64_MAX);
            }

            if (m_GraphicsQueue != VK_NULL_HANDLE)
            {
                vkQueueWaitIdle(m_GraphicsQueue);
            }

            if (m_FlushFence != VK_NULL_HANDLE)
            {
                vkDestroyFence(m_Device, m_FlushFence, 0);
                m_FlushFence = VK_NULL_HANDLE;
            }

            if (m_CommandPool != VK_NULL_HANDLE)
            {
                vkDestroyCommandPool(m_Device, m_CommandPool, 0);
                m_CommandPool = VK_NULL_HANDLE;
            }

            m_FlushCommandBuffer = VK_NULL_HANDLE;
        }

        bool EnsureBackingRenderTarget(uint32_t width, uint32_t height)
        {
            if (!m_GraphicsContext)
            {
                return false;
            }

            if (!m_BackingRenderTarget)
            {
                dmGraphics::RenderTargetCreationParams params = {};

                params.m_ColorBufferCreationParams[0].m_Type           = dmGraphics::TEXTURE_TYPE_2D;
                params.m_ColorBufferCreationParams[0].m_Width          = width;
                params.m_ColorBufferCreationParams[0].m_Height         = height;
                params.m_ColorBufferCreationParams[0].m_OriginalWidth  = width;
                params.m_ColorBufferCreationParams[0].m_OriginalHeight = height;
                params.m_ColorBufferCreationParams[0].m_MipMapCount    = 1;
                params.m_ColorBufferCreationParams[0].m_UsageHintBits  = dmGraphics::TEXTURE_USAGE_FLAG_SAMPLE |
                                                                          dmGraphics::TEXTURE_USAGE_FLAG_INPUT;

                params.m_ColorBufferParams[0].m_Data     = 0;
                params.m_ColorBufferParams[0].m_DataSize = 0;
                params.m_ColorBufferParams[0].m_Format   = dmGraphics::TEXTURE_FORMAT_BGRA8U;
                params.m_ColorBufferParams[0].m_Width    = width;
                params.m_ColorBufferParams[0].m_Height   = height;
                params.m_ColorBufferParams[0].m_Depth    = 1;

                m_BackingRenderTarget = dmGraphics::NewRenderTarget(m_GraphicsContext,
                                                                     dmGraphics::BUFFER_TYPE_COLOR0_BIT,
                                                                     params);
                if (!m_BackingRenderTarget)
                {
                    return false;
                }
            }
            else
            {
                dmGraphics::SetRenderTargetSize(m_GraphicsContext, m_BackingRenderTarget, width, height);
            }

            m_BackingTexture = dmGraphics::GetRenderTargetTexture(m_GraphicsContext,
                                                                  m_BackingRenderTarget,
                                                                  dmGraphics::BUFFER_TYPE_COLOR0_BIT);
            return m_BackingTexture != 0;
        }

        void BindTargetTexture(dmGraphics::HTexture texture)
        {
            if (!m_RenderTarget || texture == 0)
            {
                return;
            }

            VkImage image = dmGraphics::VulkanGetImage(m_GraphicsContext, texture);
            VkImageView image_view = dmGraphics::VulkanGetImageView(m_GraphicsContext, texture);

            // This currently assumes Defold hands us an image that can be treated
            // as presentable when rendering directly to the swapchain, and
            // undefined for intermediate textures. If the engine starts
            // exposing exact last-access state, this should be updated to use
            // it.
            rive::gpu::vkutil::ImageAccess initial_access;
            initial_access.pipelineStages = m_DoFinalBlit ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                                                          : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            initial_access.accessMask = VK_ACCESS_NONE;
            initial_access.layout = m_DoFinalBlit ? VK_IMAGE_LAYOUT_UNDEFINED
                                                  : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            m_RenderTarget->setTargetImageView(image_view, image, initial_access);
        }

        std::unique_ptr<rive::gpu::RenderContext>      m_RenderContext;
        rive::rcp<rive::gpu::RenderTargetVulkanImpl>   m_RenderTarget;
        dmGraphics::HContext                            m_GraphicsContext;
        dmGraphics::HRenderTarget                       m_BackingRenderTarget;
        dmGraphics::HTexture                            m_BackingTexture;
        dmGraphics::HTexture                            m_TargetTexture;
        VkDevice                                        m_Device;
        VkQueue                                         m_GraphicsQueue;
        uint32_t                                        m_GraphicsQueueFamily;
        VkCommandPool                                   m_CommandPool;
        VkCommandBuffer                                 m_FlushCommandBuffer;
        VkFence                                         m_FlushFence;
        uint64_t                                        m_FrameNumber;
        uint32_t                                        m_Width;
        uint32_t                                        m_Height;
        bool                                            m_DoFinalBlit;
    };

    IDefoldRiveRenderer* MakeDefoldRiveRendererVulkan()
    {
        return new DefoldRiveRendererVulkan();
    }
}

#endif
