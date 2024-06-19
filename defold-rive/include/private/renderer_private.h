// Copyright 2023 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_RIVE_RENDERER_PRIVATE_H
#define DM_RIVE_RENDERER_PRIVATE_H

#include "rive/pls/pls.hpp"
#include "rive/pls/pls_render_context_helper_impl.hpp"

#include "rive/pls/buffer_ring.hpp"
#include "rive/pls/pls_image.hpp"

#include <dmsdk/graphics/graphics.h>
#include <dmsdk/graphics/graphics_vulkan.h>

#include "private/shaders/shader_exports.h"

#include "defold_graphics.h"

#define RIVE_MINIFIED_SHADERS

#ifdef RIVE_MINIFIED_SHADERS
    // TODO: Nag rive to add these to the export table
    #define GLSL_gradSampler "r5"
    #define GLSL_imageSampler "n2"

    // We specify these explicitly (which is annoying) because
    // the exported symbols from rive are referring to the instance name, e.g:
    //
    // uniform buffer MyBufferType
    // {
    //     vec4 variable;
    // } my_instance;
    //
    // .. But spirv-cross doesn't give us the instance name, it only gives us the type name -_-
    #define DEFOLD_GLSL_pathBuffer     "A9"
    #define DEFOLD_GLSL_contourBuffer  "B9"
    #define DEFOLD_GLSL_paintBuffer    "V8"
    #define DEFOLD_GLSL_paintAuxBuffer "W8"
#else
    #define GLSL_gradSampler  "gradSampler"
    #define GLSL_imageSampler "imageSampler"

    #define DEFOLD_GLSL_pathBuffer     "PathBuffer"
    #define DEFOLD_GLSL_contourBuffer  "ContourBuffer"
    #define DEFOLD_GLSL_paintBuffer    "PaintBuffer"
    #define DEFOLD_GLSL_paintAuxBuffer "PaintAuxBuffer"
#endif

namespace dmRive
{
    enum RenderTargetConfig
    {
        CONFIG_NONE          = 0,
        CONFIG_SUB_PASS_LOAD = 1,
        CONFIG_RW_TEXTURE    = 2,
    };

    struct ShaderResources
    {
        dmGraphics::HVertexProgram   m_RampVs;
        dmGraphics::HFragmentProgram m_RampFs;
        dmGraphics::HVertexProgram   m_TessVs;
        dmGraphics::HFragmentProgram m_TessFs;
        dmGraphics::HVertexProgram   m_DrawPathVs;
        dmGraphics::HFragmentProgram m_DrawPathFs;
        dmGraphics::HVertexProgram   m_DrawInteriorTrianglesVs;
        dmGraphics::HFragmentProgram m_DrawInteriorTrianglesFs;
        dmGraphics::HVertexProgram   m_DrawImageMeshVs;
        dmGraphics::HFragmentProgram m_DrawImageMeshFs;
    };

    struct DefoldShaderList
    {
        dmGraphics::HProgram m_Ramp;
        dmGraphics::HProgram m_Tess;
        dmGraphics::HProgram m_DrawPath;
        dmGraphics::HProgram m_DrawInteriorTriangles;
        dmGraphics::HProgram m_DrawImageMesh;
    };

    class DefoldPLSRenderBuffer : public rive::RenderBuffer
    {
    public:
        DefoldPLSRenderBuffer(dmGraphics::HContext graphics_context, rive::RenderBufferType type, rive::RenderBufferFlags flags, size_t sizeInBytes)
        : rive::RenderBuffer(type, flags, sizeInBytes)
        , m_VertexBuffer(0)
        , m_IndexBuffer(0)
        , m_GraphicsContext(graphics_context)
        {
            dmGraphics::BufferUsage usage = (flags & rive::RenderBufferFlags::mappedOnceAtInitialization) ?
                    dmGraphics::BUFFER_USAGE_STATIC_DRAW :
                    dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW;

            if (type == rive::RenderBufferType::index)
            {
                m_IndexBuffer = dmGraphics::NewIndexBuffer(graphics_context, sizeInBytes, 0x0, usage);
            }
            else
            {
                m_VertexBuffer = dmGraphics::NewVertexBuffer(graphics_context, sizeInBytes, 0x0, usage);
            }
        }

        ~DefoldPLSRenderBuffer() override
        {
            if (m_VertexBuffer)
                dmGraphics::DeleteVertexBuffer(m_VertexBuffer);
            if (m_IndexBuffer)
                dmGraphics::DeleteIndexBuffer(m_IndexBuffer);
        }

        dmGraphics::HVertexBuffer vertexBuffer() const { return m_VertexBuffer; }
        dmGraphics::HIndexBuffer indexBuffer() const { return m_IndexBuffer; }

    protected:
        void* onMap() override
        {
            if (m_VertexBuffer)
                return dmGraphics::VulkanMapVertexBuffer(m_GraphicsContext, m_VertexBuffer, dmGraphics::BUFFER_ACCESS_WRITE_ONLY);
            else if (m_IndexBuffer)
                return dmGraphics::VulkanMapIndexBuffer(m_GraphicsContext, m_IndexBuffer, dmGraphics::BUFFER_ACCESS_WRITE_ONLY);
            assert(0);
            return 0;
        }

        void onUnmap() override
        {
            bool did_unmap = false;
            if (m_VertexBuffer)
                did_unmap = dmGraphics::VulkanUnmapVertexBuffer(m_GraphicsContext, m_VertexBuffer);
            else if (m_IndexBuffer)
                did_unmap = dmGraphics::VulkanUnmapIndexBuffer(m_GraphicsContext, m_IndexBuffer);
            assert(did_unmap);
        }
    private:
        dmGraphics::HVertexBuffer m_VertexBuffer;
        dmGraphics::HIndexBuffer  m_IndexBuffer;
        dmGraphics::HContext      m_GraphicsContext;
    };

    class DefoldPLSTexture : public rive::pls::PLSTexture
    {
    public:
        DefoldPLSTexture(dmGraphics::HContext graphics_context, uint32_t width, uint32_t height, uint32_t mipLevelCount, const uint8_t imageDataRGBA[])
        : PLSTexture(width, height)
        {
            dmGraphics::TextureCreationParams p = {};
            p.m_Width          = width;
            p.m_Height         = height;
            p.m_OriginalWidth  = p.m_Width;
            p.m_OriginalHeight = p.m_Height;
            p.m_MipMapCount    = 1; // mipLevelCount;

            m_Texture = dmGraphics::NewTexture(graphics_context, p);

            dmGraphics::TextureParams tp = {};
            tp.m_Width                   = p.m_Width;
            tp.m_Height                  = p.m_Height;
            tp.m_Format                  = dmGraphics::TEXTURE_FORMAT_RGBA;
            tp.m_Data                    = imageDataRGBA;
            tp.m_DataSize                = width * height * 4;

            dmGraphics::SetTexture(m_Texture, tp);
        }

        /*
        ~DefoldPLSTexture()
        {
            dmGraphics::DeleteTexture(m_Texture);
        }
        */

        dmGraphics::HTexture texture() const { return m_Texture; }

    private:

        dmGraphics::HTexture m_Texture;
    };

    class DefoldPLSStorageBuffer : public rive::pls::BufferRing
    {
    public:
        DefoldPLSStorageBuffer(dmGraphics::HContext graphics_context, size_t capacityInBytes)
        : BufferRing(capacityInBytes)
        , m_GraphicsContext(graphics_context)
        {
            m_StorageBuffer = dmGraphics::VulkanNewStorageBuffer(graphics_context, capacityInBytes);
        }

        dmGraphics::HStorageBuffer buffer() const { return m_StorageBuffer; }

    protected:
        void* onMapBuffer(int bufferIdx, size_t m_mapSizeInBytes) override
        {
            return dmGraphics::VulkanMapVertexBuffer(m_GraphicsContext, (dmGraphics::HVertexBuffer) m_StorageBuffer, dmGraphics::BUFFER_ACCESS_WRITE_ONLY);
        }

        void onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten) override
        {
            dmGraphics::VulkanUnmapVertexBuffer(m_GraphicsContext, (dmGraphics::HVertexBuffer) m_StorageBuffer);
        }

    private:
        dmGraphics::HStorageBuffer m_StorageBuffer;
        dmGraphics::HContext       m_GraphicsContext;
    };

    class DefoldPLSBufferRing : public rive::pls::BufferRing
    {
    public:
        DefoldPLSBufferRing(dmGraphics::HContext graphics_context, size_t capacityInBytes)
        : BufferRing(capacityInBytes)
        , m_GraphicsContext(graphics_context)
        {
            m_GPUBuffer = dmGraphics::NewVertexBuffer(graphics_context, capacityInBytes, 0x0, dmGraphics::BUFFER_USAGE_TRANSFER);
        }

        /*
        ~DefoldPLSBufferRing() override
        {
            dmGraphics::DeleteVertexBuffer(m_GPUBuffer);
        }
        */

        dmGraphics::HVertexBuffer buffer() const { return m_GPUBuffer; }

    protected:
        void* onMapBuffer(int bufferIdx, size_t m_mapSizeInBytes) override
        {
            return dmGraphics::VulkanMapVertexBuffer(m_GraphicsContext, m_GPUBuffer, dmGraphics::BUFFER_ACCESS_WRITE_ONLY);
        }

        void onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten) override
        {
            dmGraphics::VulkanUnmapVertexBuffer(m_GraphicsContext, m_GPUBuffer);
        }

    private:
        dmGraphics::HVertexBuffer m_GPUBuffer;
        dmGraphics::HContext      m_GraphicsContext;
    };

    class DefoldPLSRenderTarget : public rive::pls::PLSRenderTarget
    {
    public:
        DefoldPLSRenderTarget(dmGraphics::HContext graphics_context, RenderTargetConfig cfg, size_t width, size_t height)
        : PLSRenderTarget(width, height)
        , m_TargetTextureRW(0)
        {
            dmGraphics::RenderTargetCreationParams rt_p = {};
            m_RenderTarget = dmGraphics::NewRenderTarget(graphics_context, 0, rt_p);

            m_TargetTexture = 0; // Configured later

            dmGraphics::TextureCreationParams tcp  = {};
            tcp.m_Width                            = width;
            tcp.m_Height                           = height;
            tcp.m_OriginalWidth                    = tcp.m_Width;
            tcp.m_OriginalHeight                   = tcp.m_Height;

            if (cfg == CONFIG_SUB_PASS_LOAD)
            {
                tcp.m_UsageHintBits = dmGraphics::TEXTURE_USAGE_HINT_MEMORYLESS;
            }
            else if (cfg == CONFIG_RW_TEXTURE)
            {
                tcp.m_UsageHintBits |= dmGraphics::TEXTURE_USAGE_HINT_STORAGE | dmGraphics::TEXTURE_USAGE_HINT_COLOR;
            }

            dmGraphics::TextureParams tp = {};
            tp.m_Width                   = width;
            tp.m_Height                  = height;

            //if (cfg == CONFIG_RW_TEXTURE)
            //{
            //    dmGraphics::TextureParams _tp = tp;
            //    _tp.m_Format                  = dmGraphics::TEXTURE_FORMAT_BGRA8U;
            //
            //    m_TargetTextureRW = 1;
            //    m_TargetTexture   = dmGraphics::NewTexture(graphics_context, tcp);
            //    dmGraphics::SetTexture(m_TargetTexture, _tp);
            //}

            {
                dmGraphics::TextureParams _tp = tp;
                _tp.m_Format                  = dmGraphics::TEXTURE_FORMAT_RGBA32UI;

                m_CoverageTexture = dmGraphics::NewTexture(graphics_context, tcp);
                dmGraphics::SetTexture(m_CoverageTexture, _tp);
            }

            {
                dmGraphics::TextureParams _tp = tp;
                _tp.m_Format                  = dmGraphics::TEXTURE_FORMAT_BGRA8U;

                m_OriginalDstColorTexture = dmGraphics::NewTexture(graphics_context, tcp);
                dmGraphics::SetTexture(m_OriginalDstColorTexture, _tp);
            }

            {
                dmGraphics::TextureParams _tp = tp;
                _tp.m_Format                  = dmGraphics::TEXTURE_FORMAT_RGBA32UI;

                m_ClipTexture = dmGraphics::NewTexture(graphics_context, tcp);
                dmGraphics::SetTexture(m_ClipTexture, _tp);
            }
        }

        /*
        ~DefoldPLSRenderTarget() override {
            dmGraphics::DeleteRenderTarget(m_RenderTarget);
        }
        */

        void setTargetTexture(dmGraphics::HTexture tex)
        {
            if (!m_TargetTextureRW)
            {
                m_TargetTexture = tex;
            }
        }

    private:
        friend class DefoldPLSRenderContext;

        dmGraphics::HRenderTarget m_RenderTarget;
        dmGraphics::HTexture      m_TargetTexture;
        dmGraphics::HTexture      m_CoverageTexture;
        dmGraphics::HTexture      m_OriginalDstColorTexture;
        dmGraphics::HTexture      m_ClipTexture;
        uint8_t                   m_TargetTextureRW : 1;
    };

    static dmGraphics::HVertexBuffer defold_buffer(const rive::pls::BufferRing* bufferRing)
    {
        return static_cast<const DefoldPLSBufferRing*>(bufferRing)->buffer();
    }

    static dmGraphics::HStorageBuffer defold_storage_buffer(const rive::pls::BufferRing* bufferRing)
    {
        return static_cast<const DefoldPLSStorageBuffer*>(bufferRing)->buffer();
    }

    static void UploadEmptyTexture(dmGraphics::HTexture texture, dmGraphics::TextureFormat format, uint32_t width, uint32_t height)
    {
        dmGraphics::TextureParams tp = {};
        tp.m_Width    = width;
        tp.m_Height   = height;
        tp.m_Format   = format;
        dmGraphics::SetTexture(texture, tp);
    }

    class DefoldPLSRenderContext : public rive::pls::PLSRenderContextHelperImpl
    {
    public:
        static std::unique_ptr<rive::pls::PLSRenderContext> MakeContext(RenderTargetConfig cfg)
        {
            DefoldPLSRenderContext* plsContext = new DefoldPLSRenderContext(cfg);
            auto plsContextImpl = std::unique_ptr<DefoldPLSRenderContext>(plsContext);
            return std::make_unique<rive::pls::PLSRenderContext>(std::move(plsContextImpl));
        }

        /*
        ~DefoldPLSRenderContext() override {
            dmGraphics::DeleteTexture(m_DummyImageTexture);
        }
        */

        DefoldPLSRenderContext(RenderTargetConfig cfg)
        : m_GraphicsContext(0)
        , m_DummyImageTexture(0)
        , m_ComplexGradientPass({})
        , m_TessellationPass({})
        , m_DrawPathPass({})
        , m_InteriorTriangulationPass({})
        , m_ImageMeshPass({})
        , m_ShaderList({})
        , m_RenderTargetConfig(cfg)
        , m_AssetsInitialized(0)
        {
            // m_platformFeatures.invertOffscreenY = true;
        }

        rive::rcp<rive::RenderBuffer> makeRenderBuffer(rive::RenderBufferType type, rive::RenderBufferFlags flags, size_t sizeInBytes) override
        {
            return rive::make_rcp<DefoldPLSRenderBuffer>(vulkanContext(), type, flags, sizeInBytes);
        }

        rive::rcp<rive::pls::PLSTexture> makeImageTexture(uint32_t width,
                                         uint32_t height,
                                         uint32_t mipLevelCount,
                                         const uint8_t imageDataRGBA[]) override
        {
            return rive::make_rcp<DefoldPLSTexture>(vulkanContext(), width, height, mipLevelCount, imageDataRGBA);
        }

        void onSizeChanged(size_t width, size_t height)
        {
            m_RenderTarget = makeRenderTarget(width, height);
        }

        rive::rcp<DefoldPLSRenderTarget> renderTarget()
        {
            return m_RenderTarget;
        }

        void setDefoldContext(dmGraphics::HContext graphics_context)
        {
            m_GraphicsContext = graphics_context;
        }

        void setDefoldShaders(const DefoldShaderList& shaders)
        {
            m_ShaderList = shaders;
            m_ComplexGradientPass.m_Program = m_ShaderList.m_Ramp;
            m_TessellationPass.m_Program    = m_ShaderList.m_Tess;
        }

    protected:
        std::unique_ptr<rive::pls::BufferRing> makeVertexBufferRing(size_t capacityInBytes) override
        {
            return std::make_unique<DefoldPLSBufferRing>(m_GraphicsContext, capacityInBytes);
        }

        std::unique_ptr<rive::pls::BufferRing> makeUniformBufferRing(size_t capacityInBytes) override
        {
            return std::make_unique<DefoldPLSBufferRing>(m_GraphicsContext, capacityInBytes);
        }

        rive::rcp<DefoldPLSRenderTarget> makeRenderTarget(size_t width, size_t height)
        {
            return rive::rcp(new DefoldPLSRenderTarget(m_GraphicsContext, m_RenderTargetConfig, width, height));
        }

        /*
        enum StorageBufferStructure
        {
            uint32x4,
            uint32x2,
            float32x4,
        };
        */
        std::unique_ptr<rive::pls::BufferRing> makeStorageBufferRing(size_t capacityInBytes, rive::pls::StorageBufferStructure type) override
        {
            return std::make_unique<DefoldPLSStorageBuffer>(m_GraphicsContext, capacityInBytes);
        }

        std::unique_ptr<rive::pls::BufferRing> makeTextureTransferBufferRing(size_t capacityInBytes) override
        {
            return std::make_unique<DefoldPLSBufferRing>(m_GraphicsContext, capacityInBytes);
        }

    private:

        struct
        {
            dmGraphics::HUniformLocation   m_UniformLoc;
            dmGraphics::HProgram           m_Program;
            dmGraphics::HRenderTarget      m_RenderTarget;
            dmGraphics::HVertexDeclaration m_VertexDeclaration;
            dmGraphics::HTexture           m_Texture;
        } m_ComplexGradientPass;

        struct
        {
            dmGraphics::SetRenderTargetAttachmentsParams m_RenderTargetAttachmentParams;
            dmGraphics::HUniformLocation                 m_UniformLoc;
            dmGraphics::HUniformLocation                 m_PathSSBOLoc;
            dmGraphics::HUniformLocation                 m_ContourSSBOLoc;
            dmGraphics::HProgram                         m_Program;
            dmGraphics::HRenderTarget                    m_RenderTarget;
            dmGraphics::HVertexDeclaration               m_VertexDeclaration;
            dmGraphics::HTexture                         m_VertexTexture;
            dmGraphics::HIndexBuffer                     m_IndexBuffer;
        } m_TessellationPass;

        struct
        {
            dmGraphics::HUniformLocation   m_UniformLoc;
            dmGraphics::HUniformLocation   m_TessVertexTextureLoc;

            dmGraphics::HUniformLocation   m_PathSSBOLoc;
            dmGraphics::HUniformLocation   m_ContourSSBOLoc;

            dmGraphics::HUniformLocation   m_PaintBufferSSBOLoc;
            dmGraphics::HUniformLocation   m_PaintAuxBufferSSBOLoc;

            dmGraphics::HUniformLocation   m_GradTextureLoc;
            dmGraphics::HUniformLocation   m_ImageTextureLoc;
            dmGraphics::HUniformLocation   m_CoverageCountBufferLoc;
            dmGraphics::HUniformLocation   m_ClipBufferLoc;
            dmGraphics::HUniformLocation   m_OriginalDstColorBufferLoc;
            dmGraphics::HUniformLocation   m_FramebufferLoc;
            dmGraphics::HUniformLocation   m_GradTextureSamplerLoc;
            dmGraphics::HUniformLocation   m_ImageTextureSamplerLoc;
            dmGraphics::HVertexDeclaration m_VertexDeclaration;
            dmGraphics::HVertexBuffer      m_VertexBuffer;
            dmGraphics::HIndexBuffer       m_IndexBuffer;
        } m_DrawPathPass;

        struct
        {
            dmGraphics::HUniformLocation   m_UniformLoc;
            dmGraphics::HUniformLocation   m_PathSSBOLoc;
            dmGraphics::HUniformLocation   m_GradTextureLoc;
            dmGraphics::HUniformLocation   m_ImageTextureLoc;
            dmGraphics::HUniformLocation   m_GradTextureSamplerLoc;
            dmGraphics::HUniformLocation   m_ImageTextureSamplerLoc;
            dmGraphics::HVertexDeclaration m_VertexDeclaration;
        } m_InteriorTriangulationPass;

        struct
        {
            dmGraphics::HUniformLocation   m_UniformLoc;
            dmGraphics::HUniformLocation   m_MeshUniformsLoc;
            dmGraphics::HUniformLocation   m_ImageTextureLoc;
            dmGraphics::HUniformLocation   m_ImageTextureSamplerLoc;
            dmGraphics::HVertexDeclaration m_VertexDeclarationPosition;
            dmGraphics::HVertexDeclaration m_VertexDeclarationUv;
        } m_ImageMeshPass;

        void resizeGradientTexture(uint32_t width, uint32_t height) override
        {
            if (!m_ComplexGradientPass.m_Texture)
            {
                dmGraphics::TextureCreationParams p = {};
                p.m_Width          = width;
                p.m_Height         = height;
                p.m_OriginalWidth  = p.m_Width;
                p.m_OriginalHeight = p.m_Height;
                p.m_MipMapCount    = 1;
                p.m_UsageHintBits  = dmGraphics::TEXTURE_USAGE_HINT_SAMPLE | dmGraphics::TEXTURE_USAGE_HINT_COLOR;

                m_ComplexGradientPass.m_Texture = dmGraphics::NewTexture(m_GraphicsContext, p);

                // If the size is zero, we need to set a dummy texture the first time it's created
                if (width * height == 0)
                {
                    UploadEmptyTexture(m_ComplexGradientPass.m_Texture, dmGraphics::TEXTURE_FORMAT_RGBA, 2, 2);
                }
            }

            if (width * height > 0)
            {
                UploadEmptyTexture(m_ComplexGradientPass.m_Texture, dmGraphics::TEXTURE_FORMAT_RGBA, width, height);
            }
        }
        void resizeTessellationTexture(uint32_t width, uint32_t height) override
        {
            if (!m_TessellationPass.m_VertexTexture)
            {
                dmGraphics::TextureCreationParams p = {};
                p.m_Width          = width;
                p.m_Height         = height;
                p.m_OriginalWidth  = p.m_Width;
                p.m_OriginalHeight = p.m_Height;
                p.m_MipMapCount    = 1;
                p.m_UsageHintBits  = dmGraphics::TEXTURE_USAGE_HINT_SAMPLE | dmGraphics::TEXTURE_USAGE_HINT_COLOR;

                m_TessellationPass.m_VertexTexture = dmGraphics::NewTexture(m_GraphicsContext, p);

                // If the size is zero, we need to set a dummy texture the first time it's created
                if (width * height == 0)
                {
                    UploadEmptyTexture(m_TessellationPass.m_VertexTexture, dmGraphics::TEXTURE_FORMAT_RGBA32UI, 2, 2);
                }
            }

            if (width * height > 0)
            {
                UploadEmptyTexture(m_TessellationPass.m_VertexTexture, dmGraphics::TEXTURE_FORMAT_RGBA32UI, width, height);
            }
        }

        void InitializeGfxAssets()
        {
            // Create a dummy image texture that can be bound to the image texture slot if no texture has been specified
            {
                dmGraphics::TextureCreationParams tcp  = {};
                tcp.m_Width                            = 2;
                tcp.m_Height                           = 2;
                tcp.m_OriginalWidth                    = tcp.m_Width;
                tcp.m_OriginalHeight                   = tcp.m_Height;

                dmGraphics::TextureParams tp = {};
                tp.m_Width                   = 2;
                tp.m_Height                  = 2;

                {
                    dmGraphics::TextureParams _tp = tp;
                    _tp.m_Format                  = dmGraphics::TEXTURE_FORMAT_RGBA;

                    m_DummyImageTexture = dmGraphics::NewTexture(m_GraphicsContext, tcp);
                    dmGraphics::SetTexture(m_DummyImageTexture, _tp);
                }
            }

            //////// TESSELLATION PASS ////////
            {
                m_TessellationPass.m_IndexBuffer = dmGraphics::NewIndexBuffer(m_GraphicsContext,
                    sizeof(rive::pls::kTessSpanIndices),
                    rive::pls::kTessSpanIndices,
                    dmGraphics::BUFFER_USAGE_STATIC_DRAW);

                m_TessellationPass.m_RenderTargetAttachmentParams = {};
                m_TessellationPass.m_RenderTargetAttachmentParams.m_ColorAttachmentsCount      = 1;
                m_TessellationPass.m_RenderTargetAttachmentParams.m_ColorAttachmentLoadOps[0]  = dmGraphics::ATTACHMENT_OP_CLEAR;
                m_TessellationPass.m_RenderTargetAttachmentParams.m_ColorAttachmentStoreOps[0] = dmGraphics::ATTACHMENT_OP_STORE;

                dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(m_GraphicsContext);

                dmGraphics::AddVertexStream(stream_declaration, GLSL_a_p0p1_,          4, dmGraphics::TYPE_FLOAT,        false);
                dmGraphics::AddVertexStream(stream_declaration, GLSL_a_p2p3_,          4, dmGraphics::TYPE_FLOAT,        false);
                dmGraphics::AddVertexStream(stream_declaration, GLSL_a_joinTan_and_ys, 4, dmGraphics::TYPE_FLOAT,        false);
                dmGraphics::AddVertexStream(stream_declaration, GLSL_a_args,           4, dmGraphics::TYPE_UNSIGNED_INT, false);

                m_TessellationPass.m_VertexDeclaration = dmGraphics::NewVertexDeclaration(m_GraphicsContext, stream_declaration);
                dmGraphics::DeleteVertexStreamDeclaration(stream_declaration);

                dmGraphics::VulkanSetVertexDeclarationStepFunction(m_GraphicsContext, m_TessellationPass.m_VertexDeclaration, dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE);

                dmGraphics::RenderTargetCreationParams rt_p = {};
                rt_p.m_ColorBufferCreationParams[0].m_Type  = dmGraphics::TEXTURE_TYPE_2D;
                rt_p.m_ColorBufferParams[0].m_Format        = dmGraphics::TEXTURE_FORMAT_RGBA32UI;
                m_TessellationPass.m_RenderTarget           = dmGraphics::NewRenderTarget(m_GraphicsContext, 0, rt_p);

                m_TessellationPass.m_UniformLoc            = dmGraphics::GetUniformLocation(m_TessellationPass.m_Program, GLSL_FlushUniforms);
                m_TessellationPass.m_PathSSBOLoc           = dmGraphics::GetUniformLocation(m_TessellationPass.m_Program, DEFOLD_GLSL_pathBuffer);
                m_TessellationPass.m_ContourSSBOLoc        = dmGraphics::GetUniformLocation(m_TessellationPass.m_Program, DEFOLD_GLSL_contourBuffer);
            }

            //////// COMPLEX GRADIENT PASS ////////
            {
                dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(m_GraphicsContext);
                dmGraphics::AddVertexStream(stream_declaration, GLSL_a_span, 4, dmGraphics::TYPE_UNSIGNED_INT, false);
                m_ComplexGradientPass.m_VertexDeclaration = dmGraphics::NewVertexDeclaration(m_GraphicsContext, stream_declaration);
                dmGraphics::DeleteVertexStreamDeclaration(stream_declaration);

                dmGraphics::VulkanSetVertexDeclarationStepFunction(m_GraphicsContext, m_ComplexGradientPass.m_VertexDeclaration, dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE);

                dmGraphics::RenderTargetCreationParams rt_p = {};
                rt_p.m_ColorBufferCreationParams[0].m_Type  = dmGraphics::TEXTURE_TYPE_2D;
                rt_p.m_ColorBufferParams[0].m_Format        = dmGraphics::TEXTURE_FORMAT_RGBA;


                m_ComplexGradientPass.m_RenderTarget = dmGraphics::NewRenderTarget(m_GraphicsContext, 0, rt_p);
                m_ComplexGradientPass.m_UniformLoc   = dmGraphics::GetUniformLocation(m_ShaderList.m_Ramp, GLSL_FlushUniforms);
            }

            //////// DRAW PATH PASS ////////
            {
                dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(m_GraphicsContext);

                dmGraphics::AddVertexStream(stream_declaration, GLSL_a_patchVertexData,    4, dmGraphics::TYPE_FLOAT, false);
                dmGraphics::AddVertexStream(stream_declaration, GLSL_a_mirroredVertexData, 4, dmGraphics::TYPE_FLOAT, false);

                m_DrawPathPass.m_VertexDeclaration = dmGraphics::NewVertexDeclaration(m_GraphicsContext, stream_declaration);
                dmGraphics::DeleteVertexStreamDeclaration(stream_declaration);

                m_DrawPathPass.m_UniformLoc             = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawPath, GLSL_FlushUniforms);
                m_DrawPathPass.m_TessVertexTextureLoc   = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawPath, GLSL_tessVertexTexture);
                m_DrawPathPass.m_PathSSBOLoc            = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawPath, DEFOLD_GLSL_pathBuffer);
                m_DrawPathPass.m_ContourSSBOLoc         = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawPath, DEFOLD_GLSL_contourBuffer);
                m_DrawPathPass.m_PaintBufferSSBOLoc     = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawPath, DEFOLD_GLSL_paintBuffer);
                m_DrawPathPass.m_PaintAuxBufferSSBOLoc  = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawPath, DEFOLD_GLSL_paintAuxBuffer);
                m_DrawPathPass.m_GradTextureLoc         = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawPath, GLSL_gradTexture);
                m_DrawPathPass.m_ImageTextureLoc        = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawPath, GLSL_imageTexture);
                m_DrawPathPass.m_GradTextureSamplerLoc  = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawPath, GLSL_gradSampler);
                m_DrawPathPass.m_ImageTextureSamplerLoc = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawPath, GLSL_imageSampler);

                if (m_RenderTargetConfig == CONFIG_RW_TEXTURE)
                {

                    // TODO: THIS IS NOT VERY ROBUST!
                    //       Rive can change the order here, so we should refactor this to be a bit more stable

                    char name_buf[128] = {};
                    uint32_t num_uniforms = dmGraphics::GetUniformCount(m_ShaderList.m_DrawPath);
                    for (int i = 0; i < num_uniforms; ++i)
                    {
                        uint32_t binding = 0;
                        uint32_t set     = 0;
                        uint32_t member  = 0;
                        dmGraphics::VulkanGetUniformBinding(m_GraphicsContext, m_ShaderList.m_DrawPath, i, &set, &binding, &member);

                        dmGraphics::Type type;
                        int32_t size;
                        dmGraphics::GetUniformName(m_ShaderList.m_DrawPath, i, name_buf, sizeof(name_buf), &type, &size);

                        if (set == 2)
                        {
                            switch(binding)
                            {
                            case 0:
                                m_DrawPathPass.m_FramebufferLoc = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawPath, name_buf);
                                break;
                            case 1:
                                m_DrawPathPass.m_CoverageCountBufferLoc = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawPath, name_buf);
                                break;
                            case 2:
                                m_DrawPathPass.m_ClipBufferLoc = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawPath, name_buf);
                                break;
                            case 3:
                                m_DrawPathPass.m_OriginalDstColorBufferLoc = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawPath, name_buf);
                                break;
                            }
                        }
                    }
                }

                uint32_t patchVertexMemorySize = rive::pls::kPatchVertexBufferCount * sizeof(rive::pls::PatchVertex);
                uint32_t patchIndexMemorySize = rive::pls::kPatchIndexBufferCount * sizeof(uint16_t);

                void* patchVertexMemory = malloc(patchVertexMemorySize);
                void* patchIndexMemory = malloc(patchIndexMemorySize);

                GeneratePatchBufferData(reinterpret_cast<rive::pls::PatchVertex*>(patchVertexMemory),
                                        reinterpret_cast<uint16_t*>(patchIndexMemory));

                m_DrawPathPass.m_VertexBuffer = dmGraphics::NewVertexBuffer(m_GraphicsContext, patchVertexMemorySize, patchVertexMemory, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                m_DrawPathPass.m_IndexBuffer  = dmGraphics::NewIndexBuffer(m_GraphicsContext, patchIndexMemorySize, patchIndexMemory, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

                free(patchVertexMemory);
                free(patchIndexMemory);
            }

            //////// INTERIOR TRIANGULATION PASS ////////
            {
                dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(m_GraphicsContext);
                dmGraphics::AddVertexStream(stream_declaration, GLSL_a_triangleVertex, 3, dmGraphics::TYPE_FLOAT, false);
                m_InteriorTriangulationPass.m_VertexDeclaration = dmGraphics::NewVertexDeclaration(m_GraphicsContext, stream_declaration);
                dmGraphics::DeleteVertexStreamDeclaration(stream_declaration);

                m_InteriorTriangulationPass.m_UniformLoc      = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawInteriorTriangles, GLSL_FlushUniforms);
                m_InteriorTriangulationPass.m_PathSSBOLoc  = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawInteriorTriangles, DEFOLD_GLSL_pathBuffer);
                m_InteriorTriangulationPass.m_GradTextureLoc  = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawInteriorTriangles, GLSL_gradTexture);
                m_InteriorTriangulationPass.m_ImageTextureLoc = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawInteriorTriangles, GLSL_imageTexture);
                m_InteriorTriangulationPass.m_GradTextureSamplerLoc  = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawInteriorTriangles, GLSL_gradSampler);
                m_InteriorTriangulationPass.m_ImageTextureSamplerLoc = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawInteriorTriangles, GLSL_imageSampler);
            }

            //////// IMAGE MESH PASS ////////
            {
                dmGraphics::HVertexStreamDeclaration stream_declaration_pos = dmGraphics::NewVertexStreamDeclaration(m_GraphicsContext);
                dmGraphics::AddVertexStream(stream_declaration_pos, GLSL_a_position, 2, dmGraphics::TYPE_FLOAT, false);
                m_ImageMeshPass.m_VertexDeclarationPosition = dmGraphics::NewVertexDeclaration(m_GraphicsContext, stream_declaration_pos);
                dmGraphics::DeleteVertexStreamDeclaration(stream_declaration_pos);

                dmGraphics::HVertexStreamDeclaration stream_declaration_uv = dmGraphics::NewVertexStreamDeclaration(m_GraphicsContext);
                dmGraphics::AddVertexStream(stream_declaration_uv, GLSL_a_texCoord, 2, dmGraphics::TYPE_FLOAT, false);
                m_ImageMeshPass.m_VertexDeclarationUv = dmGraphics::NewVertexDeclaration(m_GraphicsContext, stream_declaration_uv);
                dmGraphics::DeleteVertexStreamDeclaration(stream_declaration_uv);

                m_ImageMeshPass.m_UniformLoc             = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawImageMesh, GLSL_FlushUniforms);
                m_ImageMeshPass.m_MeshUniformsLoc        = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawImageMesh, GLSL_ImageDrawUniforms);
                m_ImageMeshPass.m_ImageTextureLoc        = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawImageMesh, GLSL_imageTexture);
                m_ImageMeshPass.m_ImageTextureSamplerLoc = dmGraphics::GetUniformLocation(m_ShaderList.m_DrawImageMesh, GLSL_imageSampler);
            }

            m_AssetsInitialized = true;
        }

        void flush(const rive::pls::FlushDescriptor& desc) override
        {
            // We are not ready to flush yet
            if (m_GraphicsContext == 0x0)
            {
                return;
            }

            dmGraphics::PipelineState ps = dmGraphics::GetPipelineState(m_GraphicsContext);

            if (!m_AssetsInitialized)
            {
                InitializeGfxAssets();
            }

            const DefoldPLSRenderTarget* renderTarget      = static_cast<const DefoldPLSRenderTarget*>(desc.renderTarget);
            const dmGraphics::HVertexBuffer tessSpan       = defold_buffer(tessSpanBufferRing());
            const dmGraphics::HVertexBuffer flushUniforms  = defold_buffer(flushUniformBufferRing());
            const dmGraphics::HVertexBuffer triangleBuffer = defold_buffer(triangleBufferRing());
            const dmGraphics::HVertexBuffer gradSpanBuffer = defold_buffer(gradSpanBufferRing());

            const dmGraphics::HStorageBuffer pathSSBO             = defold_storage_buffer(pathBufferRing());
            const dmGraphics::HStorageBuffer contourSSBO          = defold_storage_buffer(contourBufferRing());
            const dmGraphics::HStorageBuffer paintBufferSSBO      = defold_storage_buffer(paintBufferRing());
            const dmGraphics::HStorageBuffer paintAuxBufferSSBO   = defold_storage_buffer(paintAuxBufferRing());

            dmGraphics::DisableState(m_GraphicsContext, dmGraphics::STATE_BLEND);

            if (desc.complexGradSpanCount > 0)
            {
                dmGraphics::SetRenderTargetAttachmentsParams setRenderTargetParams = {};
                setRenderTargetParams.m_Width                                      = rive::pls::kGradTextureWidth;
                setRenderTargetParams.m_Height                                     = desc.complexGradRowsTop + desc.complexGradRowsHeight;
                setRenderTargetParams.m_SetDimensions                              = 1;
                setRenderTargetParams.m_ColorAttachmentsCount                      = 1;
                setRenderTargetParams.m_ColorAttachments[0]                        = m_ComplexGradientPass.m_Texture;
                setRenderTargetParams.m_ColorAttachmentLoadOps[0]                  = dmGraphics::ATTACHMENT_OP_LOAD;
                setRenderTargetParams.m_ColorAttachmentStoreOps[0]                 = dmGraphics::ATTACHMENT_OP_STORE;

                dmGraphics::VulkanSetRenderTargetAttachments(m_GraphicsContext, m_ComplexGradientPass.m_RenderTarget, setRenderTargetParams);
                dmGraphics::SetRenderTarget(m_GraphicsContext, m_ComplexGradientPass.m_RenderTarget, 0);

                dmGraphics::EnableState(m_GraphicsContext, dmGraphics::STATE_CULL_FACE);
                dmGraphics::SetCullFace(m_GraphicsContext, dmGraphics::FACE_TYPE_FRONT);

                dmGraphics::SetViewport(m_GraphicsContext, 0, desc.complexGradRowsTop, rive::pls::kGradTextureWidth, desc.complexGradRowsHeight);

                dmGraphics::EnableProgram(m_GraphicsContext, m_ComplexGradientPass.m_Program);

                dmGraphics::EnableVertexBuffer(m_GraphicsContext, gradSpanBuffer, 0);
                dmGraphics::EnableVertexDeclaration(m_GraphicsContext, m_ComplexGradientPass.m_VertexDeclaration, 0, m_ComplexGradientPass.m_Program);

                dmGraphics::VulkanSetConstantBuffer(m_GraphicsContext, flushUniforms, 0, m_ComplexGradientPass.m_UniformLoc);

                dmGraphics::VulkanDrawBaseInstance(m_GraphicsContext, dmGraphics::PRIMITIVE_TRIANGLE_STRIP, 0, 4, desc.complexGradSpanCount, 0);
            }

            if (desc.simpleGradTexelsHeight > 0)
            {
                const dmGraphics::HVertexBuffer simpleColorRamp = defold_buffer(simpleColorRampsBufferRing());

                dmGraphics::TextureParams p = {};
                p.m_Width                   = desc.simpleGradTexelsWidth;
                p.m_Height                  = desc.simpleGradTexelsHeight;
                p.m_SubUpdate               = 1;

                dmGraphics::VulkanCopyBufferToTexture(m_GraphicsContext, simpleColorRamp, m_ComplexGradientPass.m_Texture, desc.simpleGradTexelsWidth, desc.simpleGradTexelsHeight);
            }

            if (desc.tessVertexSpanCount > 0)
            {
                // Target state
                m_TessellationPass.m_RenderTargetAttachmentParams.m_ColorAttachments[0] = m_TessellationPass.m_VertexTexture;
                dmGraphics::VulkanSetRenderTargetAttachments(m_GraphicsContext, m_TessellationPass.m_RenderTarget, m_TessellationPass.m_RenderTargetAttachmentParams);
                dmGraphics::SetRenderTarget(m_GraphicsContext, m_TessellationPass.m_RenderTarget, 0);

                // Pipeline state, we apparently flip front and back culling in the engine?
                dmGraphics::EnableState(m_GraphicsContext, dmGraphics::STATE_CULL_FACE);
                dmGraphics::SetCullFace(m_GraphicsContext, dmGraphics::FACE_TYPE_FRONT);

                dmGraphics::SetViewport(m_GraphicsContext, 0, 0, rive::pls::kTessTextureWidth, desc.tessDataHeight);
                dmGraphics::EnableProgram(m_GraphicsContext, m_TessellationPass.m_Program);

                // Bindings
                dmGraphics::VulkanSetConstantBuffer(m_GraphicsContext, flushUniforms, desc.flushUniformDataOffsetInBytes, m_TessellationPass.m_UniformLoc);

                dmGraphics::VulkanSetStorageBuffer(m_GraphicsContext, pathSSBO,    0, desc.firstPath * sizeof(rive::pls::PathData), m_TessellationPass.m_PathSSBOLoc);
                dmGraphics::VulkanSetStorageBuffer(m_GraphicsContext, contourSSBO, 1, desc.firstContour * sizeof(rive::pls::ContourData), m_TessellationPass.m_ContourSSBOLoc);

                // Geometry
                dmGraphics::EnableVertexBuffer(m_GraphicsContext, tessSpan, 0);
                dmGraphics::EnableVertexDeclaration(m_GraphicsContext, m_TessellationPass.m_VertexDeclaration, 0, m_TessellationPass.m_Program);
                dmGraphics::VulkanDrawElementsInstanced(m_GraphicsContext,
                    dmGraphics::PRIMITIVE_TRIANGLES,
                    0,
                    std::size(rive::pls::kTessSpanIndices),
                    desc.tessVertexSpanCount,
                    desc.firstTessVertexSpan,
                    dmGraphics::TYPE_UNSIGNED_SHORT,
                    m_TessellationPass.m_IndexBuffer);
            }

            dmGraphics::SetRenderTargetAttachmentsParams setRenderTargetParams = {};

            if (m_RenderTargetConfig == RenderTargetConfig::CONFIG_SUB_PASS_LOAD)
            {
                setRenderTargetParams.m_ColorAttachmentsCount = 4;

                setRenderTargetParams.m_ColorAttachments[0] = renderTarget->m_TargetTexture;
                setRenderTargetParams.m_ColorAttachments[1] = renderTarget->m_CoverageTexture;
                setRenderTargetParams.m_ColorAttachments[3] = renderTarget->m_OriginalDstColorTexture;
                setRenderTargetParams.m_ColorAttachments[2] = renderTarget->m_ClipTexture;

                setRenderTargetParams.m_ColorAttachmentLoadOps[0] = dmGraphics::ATTACHMENT_OP_LOAD;
                setRenderTargetParams.m_ColorAttachmentLoadOps[1] = dmGraphics::ATTACHMENT_OP_CLEAR;
                setRenderTargetParams.m_ColorAttachmentLoadOps[3] = dmGraphics::ATTACHMENT_OP_DONT_CARE;
                setRenderTargetParams.m_ColorAttachmentLoadOps[2] = dmGraphics::ATTACHMENT_OP_CLEAR;

                setRenderTargetParams.m_ColorAttachmentStoreOps[0] = dmGraphics::ATTACHMENT_OP_STORE;
                setRenderTargetParams.m_ColorAttachmentStoreOps[1] = dmGraphics::ATTACHMENT_OP_DONT_CARE;
                setRenderTargetParams.m_ColorAttachmentStoreOps[3] = dmGraphics::ATTACHMENT_OP_DONT_CARE;
                setRenderTargetParams.m_ColorAttachmentStoreOps[2] = dmGraphics::ATTACHMENT_OP_DONT_CARE;

                dmGraphics::VulkanSetRenderTargetAttachments(m_GraphicsContext, renderTarget->m_RenderTarget, setRenderTargetParams);
            }
            else if (m_RenderTargetConfig == RenderTargetConfig::CONFIG_RW_TEXTURE)
            {
                setRenderTargetParams.m_SetDimensions = 1;
                setRenderTargetParams.m_Width         = renderTarget->width();
                setRenderTargetParams.m_Height        = renderTarget->height();

                float clear_value[4] = {};
                dmGraphics::VulkanClearTexture(m_GraphicsContext, renderTarget->m_TargetTexture, clear_value);
                dmGraphics::VulkanClearTexture(m_GraphicsContext, renderTarget->m_CoverageTexture, clear_value);
                dmGraphics::VulkanClearTexture(m_GraphicsContext, renderTarget->m_OriginalDstColorTexture, clear_value);
                dmGraphics::VulkanClearTexture(m_GraphicsContext, renderTarget->m_ClipTexture, clear_value);

                dmGraphics::VulkanSetRenderTargetAttachments(m_GraphicsContext, renderTarget->m_RenderTarget, setRenderTargetParams);

                /*
                setRenderTargetParams.m_ColorAttachmentsCount = 4;

                setRenderTargetParams.m_ColorAttachments[0] = renderTarget->m_TargetTexture;
                setRenderTargetParams.m_ColorAttachments[1] = renderTarget->m_CoverageTexture;
                setRenderTargetParams.m_ColorAttachments[2] = renderTarget->m_OriginalDstColorTexture;
                setRenderTargetParams.m_ColorAttachments[3] = renderTarget->m_ClipTexture;

                setRenderTargetParams.m_ColorAttachmentLoadOps[0] = dmGraphics::ATTACHMENT_OP_LOAD;
                setRenderTargetParams.m_ColorAttachmentLoadOps[1] = dmGraphics::ATTACHMENT_OP_CLEAR;
                setRenderTargetParams.m_ColorAttachmentLoadOps[2] = dmGraphics::ATTACHMENT_OP_DONT_CARE;
                setRenderTargetParams.m_ColorAttachmentLoadOps[3] = dmGraphics::ATTACHMENT_OP_CLEAR;

                setRenderTargetParams.m_ColorAttachmentStoreOps[0] = dmGraphics::ATTACHMENT_OP_STORE;
                setRenderTargetParams.m_ColorAttachmentStoreOps[1] = dmGraphics::ATTACHMENT_OP_DONT_CARE;
                setRenderTargetParams.m_ColorAttachmentStoreOps[2] = dmGraphics::ATTACHMENT_OP_DONT_CARE;
                setRenderTargetParams.m_ColorAttachmentStoreOps[3] = dmGraphics::ATTACHMENT_OP_DONT_CARE;

                dmGraphics::VulkanSetRenderTargetAttachments(m_GraphicsContext, renderTarget->m_RenderTarget, setRenderTargetParams);

                uint8_t sub_pass_0_color_attachments[] = { dmGraphics::SUBPASS_ATTACHMENT_UNUSED, dmGraphics::SUBPASS_ATTACHMENT_UNUSED, dmGraphics::SUBPASS_ATTACHMENT_UNUSED, dmGraphics::SUBPASS_ATTACHMENT_UNUSED };

                dmGraphics::CreateRenderPassParams createRenderPassParams           = {};
                createRenderPassParams.m_SubPassCount                               = 1;
                createRenderPassParams.m_SubPasses[0].m_ColorAttachmentIndices      = sub_pass_0_color_attachments;
                createRenderPassParams.m_SubPasses[0].m_ColorAttachmentIndicesCount = DM_ARRAY_SIZE(sub_pass_0_color_attachments);
                createRenderPassParams.m_DependencyCount                            = 1;
                createRenderPassParams.m_Dependencies[0].m_Src                      = 0;
                createRenderPassParams.m_Dependencies[0].m_Dst                      = 0;

                dmGraphics::VulkanCreateRenderPass(m_GraphicsContext, renderTarget->m_RenderTarget, createRenderPassParams);
                */
            }

            dmGraphics::SetRenderTarget(m_GraphicsContext, renderTarget->m_RenderTarget, 0);

            dmGraphics::SetViewport(m_GraphicsContext, 0, 0, renderTarget->width(), renderTarget->height());

            size_t meshDataOffset = 0;

            for (const rive::pls::DrawBatch& draw : *desc.drawList)
            {
                if (draw.elementCount == 0)
                {
                    continue;
                }

                rive::pls::DrawType drawType = draw.drawType;

                dmGraphics::HTexture imageTexture = m_DummyImageTexture;

                // Bind the appropriate image texture, if any.
                if (auto tex = static_cast<const DefoldPLSTexture*>(draw.imageTexture))
                {
                    imageTexture = tex->texture();
                }

                switch (drawType)
                {
                    case rive::pls::DrawType::midpointFanPatches:
                    case rive::pls::DrawType::outerCurvePatches:
                    {
                        dmGraphics::EnableState(m_GraphicsContext, dmGraphics::STATE_CULL_FACE);
                        dmGraphics::SetCullFace(m_GraphicsContext, dmGraphics::FACE_TYPE_FRONT);

                        dmGraphics::EnableProgram(m_GraphicsContext, m_ShaderList.m_DrawPath);

                        dmGraphics::VulkanSetStorageBuffer(m_GraphicsContext, pathSSBO,           0, 0, m_DrawPathPass.m_PathSSBOLoc);
                        dmGraphics::VulkanSetStorageBuffer(m_GraphicsContext, paintBufferSSBO,    1, 0, m_DrawPathPass.m_PaintBufferSSBOLoc);
                        dmGraphics::VulkanSetStorageBuffer(m_GraphicsContext, paintAuxBufferSSBO, 2, 0, m_DrawPathPass.m_PaintAuxBufferSSBOLoc);
                        dmGraphics::VulkanSetStorageBuffer(m_GraphicsContext, contourSSBO,        3, 0, m_DrawPathPass.m_ContourSSBOLoc);

                        dmGraphics::VulkanSetConstantBuffer(m_GraphicsContext, flushUniforms, 0, m_DrawPathPass.m_UniformLoc);
                        dmGraphics::EnableTexture(m_GraphicsContext, 0, 0, m_TessellationPass.m_VertexTexture);
                        dmGraphics::EnableTexture(m_GraphicsContext, 1, 0, m_ComplexGradientPass.m_Texture);
                        dmGraphics::EnableTexture(m_GraphicsContext, 2, 0, imageTexture);

                        dmGraphics::SetSampler(m_GraphicsContext, m_DrawPathPass.m_TessVertexTextureLoc, 0);
                        dmGraphics::SetSampler(m_GraphicsContext, m_DrawPathPass.m_GradTextureLoc,       1);
                        dmGraphics::SetSampler(m_GraphicsContext, m_DrawPathPass.m_ImageTextureLoc,      2);

                        dmGraphics::SetSampler(m_GraphicsContext, m_DrawPathPass.m_GradTextureSamplerLoc,  1);
                        dmGraphics::SetSampler(m_GraphicsContext, m_DrawPathPass.m_ImageTextureSamplerLoc, 2);

                        if (m_RenderTargetConfig == CONFIG_RW_TEXTURE)
                        {
                            dmGraphics::EnableTexture(m_GraphicsContext, 3, 0, renderTarget->m_TargetTexture);
                            dmGraphics::EnableTexture(m_GraphicsContext, 4, 0, renderTarget->m_CoverageTexture);
                            dmGraphics::EnableTexture(m_GraphicsContext, 5, 0, renderTarget->m_OriginalDstColorTexture);
                            dmGraphics::EnableTexture(m_GraphicsContext, 6, 0, renderTarget->m_ClipTexture);

                            dmGraphics::SetSampler(m_GraphicsContext, m_DrawPathPass.m_FramebufferLoc,            3);
                            dmGraphics::SetSampler(m_GraphicsContext, m_DrawPathPass.m_CoverageCountBufferLoc,    4);
                            dmGraphics::SetSampler(m_GraphicsContext, m_DrawPathPass.m_OriginalDstColorBufferLoc, 5);
                            dmGraphics::SetSampler(m_GraphicsContext, m_DrawPathPass.m_ClipBufferLoc,             6);
                        }

                        dmGraphics::SetTextureParams(m_TessellationPass.m_VertexTexture, dmGraphics::TEXTURE_FILTER_NEAREST, dmGraphics::TEXTURE_FILTER_NEAREST, dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE, dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE, 0.0f);

                        dmGraphics::EnableVertexBuffer(m_GraphicsContext, m_DrawPathPass.m_VertexBuffer, 0);
                        dmGraphics::EnableVertexDeclaration(m_GraphicsContext, m_DrawPathPass.m_VertexDeclaration, 0, m_ShaderList.m_DrawPath);

                        dmGraphics::VulkanDrawElementsInstanced(m_GraphicsContext,
                            dmGraphics::PRIMITIVE_TRIANGLES,
                            PatchBaseIndex(drawType) * sizeof(uint16_t),
                            PatchIndexCount(drawType),
                            draw.elementCount,
                            draw.baseElement,
                            dmGraphics::TYPE_UNSIGNED_SHORT,
                            m_DrawPathPass.m_IndexBuffer);
                        break;
                    }
                    case rive::pls::DrawType::interiorTriangulation:
                    {
                        dmGraphics::EnableState(m_GraphicsContext, dmGraphics::STATE_CULL_FACE);
                        dmGraphics::SetCullFace(m_GraphicsContext, dmGraphics::FACE_TYPE_FRONT);

                        dmGraphics::EnableVertexBuffer(m_GraphicsContext, triangleBuffer, 0);
                        dmGraphics::EnableVertexDeclaration(m_GraphicsContext, m_InteriorTriangulationPass.m_VertexDeclaration, 0, m_ShaderList.m_DrawInteriorTriangles);

                        dmGraphics::VulkanSetStorageBuffer(m_GraphicsContext, pathSSBO, 0, 0, m_InteriorTriangulationPass.m_PathSSBOLoc);

                        dmGraphics::EnableProgram(m_GraphicsContext, m_ShaderList.m_DrawInteriorTriangles);
                        dmGraphics::EnableTexture(m_GraphicsContext, 0, 0, m_ComplexGradientPass.m_Texture);
                        dmGraphics::EnableTexture(m_GraphicsContext, 1, 0, imageTexture);

                        dmGraphics::VulkanSetConstantBuffer(m_GraphicsContext, flushUniforms, 0, m_InteriorTriangulationPass.m_UniformLoc);
                        dmGraphics::SetSampler(m_GraphicsContext, m_InteriorTriangulationPass.m_GradTextureLoc,  0);
                        dmGraphics::SetSampler(m_GraphicsContext, m_InteriorTriangulationPass.m_ImageTextureLoc, 1);
                        dmGraphics::SetSampler(m_GraphicsContext, m_InteriorTriangulationPass.m_GradTextureSamplerLoc,  0);
                        dmGraphics::SetSampler(m_GraphicsContext, m_InteriorTriangulationPass.m_ImageTextureSamplerLoc, 1);

                        if (m_RenderTargetConfig == CONFIG_RW_TEXTURE)
                        {
                            dmGraphics::EnableTexture(m_GraphicsContext, 2, 0, renderTarget->m_TargetTexture);
                            dmGraphics::EnableTexture(m_GraphicsContext, 3, 0, renderTarget->m_CoverageTexture);
                            dmGraphics::EnableTexture(m_GraphicsContext, 4, 0, renderTarget->m_OriginalDstColorTexture);
                            dmGraphics::EnableTexture(m_GraphicsContext, 5, 0, renderTarget->m_ClipTexture);

                            dmGraphics::SetSampler(m_GraphicsContext, m_DrawPathPass.m_FramebufferLoc,            2);
                            dmGraphics::SetSampler(m_GraphicsContext, m_DrawPathPass.m_CoverageCountBufferLoc,    3);
                            dmGraphics::SetSampler(m_GraphicsContext, m_DrawPathPass.m_OriginalDstColorBufferLoc, 4);
                            dmGraphics::SetSampler(m_GraphicsContext, m_DrawPathPass.m_ClipBufferLoc,             5);

                            dmGraphics::VulkanMemorybarrier(m_GraphicsContext, renderTarget->m_TargetTexture,
                                dmGraphics::STAGE_FLAG_FRAGMENT_SHADER,
                                dmGraphics::STAGE_FLAG_FRAGMENT_SHADER,
                                dmGraphics::ACCESS_FLAG_WRITE | dmGraphics::ACCESS_FLAG_SHADER,
                                dmGraphics::ACCESS_FLAG_READ | dmGraphics::ACCESS_FLAG_SHADER);
                        }

                        dmGraphics::VulkanDrawBaseInstance(m_GraphicsContext, dmGraphics::PRIMITIVE_TRIANGLES, draw.baseElement, draw.elementCount, 1, 0);

                        if (m_RenderTargetConfig == CONFIG_RW_TEXTURE)
                        {
                            dmGraphics::VulkanMemorybarrier(m_GraphicsContext, renderTarget->m_TargetTexture,
                                dmGraphics::STAGE_FLAG_FRAGMENT_SHADER,
                                dmGraphics::STAGE_FLAG_FRAGMENT_SHADER,
                                dmGraphics::ACCESS_FLAG_WRITE | dmGraphics::ACCESS_FLAG_SHADER,
                                dmGraphics::ACCESS_FLAG_READ | dmGraphics::ACCESS_FLAG_SHADER);
                        }
                        break;
                    }
                    case rive::pls::DrawType::imageMesh:
                    {
                        auto vertexBuffer = static_cast<const DefoldPLSRenderBuffer*>(draw.vertexBuffer);
                        auto uvBuffer     = static_cast<const DefoldPLSRenderBuffer*>(draw.uvBuffer);
                        auto indexBuffer  = static_cast<const DefoldPLSRenderBuffer*>(draw.indexBuffer);

                        dmGraphics::HVertexBuffer gfx_vertex_buffer = vertexBuffer->vertexBuffer();
                        dmGraphics::HVertexBuffer gfx_uv_buffer = uvBuffer->vertexBuffer();

                        const rive::pls::BufferRing* plsImageMeshUniforms = imageDrawUniformBufferRing();
                        const dmGraphics::HVertexBuffer imageMeshUniforms = defold_buffer(plsImageMeshUniforms);

                        dmGraphics::EnableProgram(m_GraphicsContext, m_ShaderList.m_DrawImageMesh);

                        dmGraphics::EnableVertexBuffer(m_GraphicsContext, gfx_vertex_buffer, 0);
                        dmGraphics::EnableVertexBuffer(m_GraphicsContext, gfx_uv_buffer, 1);

                        dmGraphics::EnableVertexDeclaration(m_GraphicsContext, m_ImageMeshPass.m_VertexDeclarationPosition, 0, m_ShaderList.m_DrawImageMesh);
                        dmGraphics::EnableVertexDeclaration(m_GraphicsContext, m_ImageMeshPass.m_VertexDeclarationUv, 1, m_ShaderList.m_DrawImageMesh);

                        dmGraphics::DisableState(m_GraphicsContext, dmGraphics::STATE_CULL_FACE);

                        dmGraphics::VulkanSetConstantBuffer(m_GraphicsContext, flushUniforms, 0, m_ImageMeshPass.m_UniformLoc);
                        dmGraphics::VulkanSetConstantBuffer(m_GraphicsContext, imageMeshUniforms, draw.imageDrawDataOffset, m_ImageMeshPass.m_MeshUniformsLoc);

                        dmGraphics::EnableTexture(m_GraphicsContext, 0, 0, imageTexture);
                        dmGraphics::SetSampler(m_GraphicsContext, m_ImageMeshPass.m_ImageTextureLoc,        0);
                        dmGraphics::SetSampler(m_GraphicsContext, m_ImageMeshPass.m_ImageTextureSamplerLoc, 0);

                        if (m_RenderTargetConfig == CONFIG_RW_TEXTURE)
                        {
                            dmGraphics::EnableTexture(m_GraphicsContext, 1, 0, renderTarget->m_TargetTexture);
                            dmGraphics::EnableTexture(m_GraphicsContext, 2, 0, renderTarget->m_ClipTexture);

                            dmGraphics::SetSampler(m_GraphicsContext, m_DrawPathPass.m_FramebufferLoc, 1);
                            dmGraphics::SetSampler(m_GraphicsContext, m_DrawPathPass.m_ClipBufferLoc,  2);
                        }

                        dmGraphics::DrawElements(m_GraphicsContext,
                            dmGraphics::PRIMITIVE_TRIANGLES,
                            draw.baseElement * sizeof(uint16_t),
                            draw.elementCount,
                            dmGraphics::TYPE_UNSIGNED_SHORT, indexBuffer->indexBuffer());

                        dmGraphics::DisableVertexDeclaration(m_GraphicsContext, m_ImageMeshPass.m_VertexDeclarationPosition);
                        dmGraphics::DisableVertexDeclaration(m_GraphicsContext, m_ImageMeshPass.m_VertexDeclarationUv);

                        dmGraphics::DisableVertexBuffer(m_GraphicsContext, gfx_vertex_buffer);
                        dmGraphics::DisableVertexBuffer(m_GraphicsContext, gfx_uv_buffer);
                        break;
                    }
                }
            }

            dmGraphics::SetRenderTarget(m_GraphicsContext, 0, 0);

            dmGraphics::DisableTexture(m_GraphicsContext, 0, 0);
            dmGraphics::DisableTexture(m_GraphicsContext, 1, 0);
            dmGraphics::DisableTexture(m_GraphicsContext, 2, 0);
            dmGraphics::DisableTexture(m_GraphicsContext, 3, 0);
            dmGraphics::DisableTexture(m_GraphicsContext, 4, 0);

            dmGraphics::VulkanSetPipelineState(m_GraphicsContext, &ps);
        }

        inline dmGraphics::HContext vulkanContext() { return m_GraphicsContext ? m_GraphicsContext : dmGraphics::VulkanGetContext(); }

        rive::rcp<DefoldPLSRenderTarget> m_RenderTarget;
        dmGraphics::HContext             m_GraphicsContext;
        dmGraphics::HTexture             m_DummyImageTexture;
        DefoldShaderList                 m_ShaderList;
        RenderTargetConfig               m_RenderTargetConfig;
        bool                             m_AssetsInitialized;
    };
} // namespace rive::pls



#endif // DM_RIVE_RENDERER_PRIVATE_H
