
#ifdef DM_RIVE_USE_OPENGL


#if defined(RIVE_ANDROID) || defined(RIVE_WEBGL)
    #undef GL_ES_VERSION_2_0
    #undef GL_ES_VERSION_3_0

    #include <GLES3/gl3.h>
#endif

#include <dmsdk/graphics/graphics_native.h>
#include <dmsdk/graphics/graphics_opengl.h>
#include <dmsdk/dlib/log.h>

#include "renderer_context.h"

// NOTE FOR LINUX:
// If there are issues linking linux (opengl/glad related symbols already defined),
// the current solution is to define GLAPI with extern linkage in glad_custom.h
// At some point it would be nice to have a proper solution, but for now it's good enough..
#if defined(RIVE_LINUX)
    #if defined(GLAPI)
        #undef GLAPI
    #endif
    #define GLAPI extern "C"
#endif

#include <rive/renderer/rive_renderer.hpp>
#include <rive/renderer/texture.hpp>
#include <rive/renderer/gl/render_context_gl_impl.hpp>
#include <rive/renderer/gl/render_target_gl.hpp>

#include <defold/defold_graphics.h>

#ifdef RIVE_DESKTOP_GL
    #define GLFW_INCLUDE_NONE
    #include "GLFW/glfw3.h"

    #include <glad/gles2.h>
#endif

static void OpenGLCheckError(const char* context)
{
    GLint err = glGetError();
    bool status_ok = err == 0;
    while (err != 0)
    {
        dmLogInfo("%s - OpenGL Error %d", context, err);
        err = glGetError();
    }
    assert(status_ok);
}

namespace dmGraphics
{
    // TODO: DMSDK?
    enum BufferType
    {
        BUFFER_TYPE_COLOR0_BIT  = 0x01,
        BUFFER_TYPE_COLOR1_BIT  = 0x02,
        BUFFER_TYPE_COLOR2_BIT  = 0x04,
        BUFFER_TYPE_COLOR3_BIT  = 0x08,
        BUFFER_TYPE_DEPTH_BIT   = 0x10,
        BUFFER_TYPE_STENCIL_BIT = 0x20,
    };

    HTexture GetRenderTargetTexture(HRenderTarget render_target, BufferType buffer_type);
};

namespace dmRive
{
    class DefoldRiveRendererOpenGL : public IDefoldRiveRenderer
    {
    public:
        DefoldRiveRendererOpenGL()
        {
        #ifdef RIVE_DESKTOP_GL
            // Load the OpenGL API using glad.
            if (!gladLoadCustomLoader((GLADloadfunc)glfwGetProcAddress))
            {
                fprintf(stderr, "Failed to initialize glad.\n");
                abort();
            }
        #endif

            dmLogInfo("==== GL GPU: %s ====\n", glGetString(GL_RENDERER));

            m_DefoldRenderTarget = 0;

            m_RenderContext = rive::gpu::RenderContextGLImpl::MakeContext({
                .disableFragmentShaderInterlock = false // options.disableRasterOrdering,
            });
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
            m_DefoldPipelineState = dmGraphics::GetPipelineState(m_GraphicsContext);
            m_RenderContext->static_impl_cast<rive::gpu::RenderContextGLImpl>()->invalidateGLState();
            m_RenderContext->beginFrame(frameDescriptor);
            OpenGLCheckError("BeginFrame After");
        }

        void Flush() override
        {
            m_RenderContext->flush({.renderTarget = m_RenderTarget.get()});
            m_RenderContext->static_impl_cast<rive::gpu::RenderContextGLImpl>()->unbindGLInternalResources();
            OpenGLCheckError("Flush After");

            // TODO: We should bind the currently bound target
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Rive messes up the state after flush it seems.
            SetDefoldGraphicsState(dmGraphics::STATE_CULL_FACE, m_DefoldPipelineState.m_CullFaceEnabled);
            SetDefoldGraphicsState(dmGraphics::STATE_BLEND, m_DefoldPipelineState.m_BlendEnabled);

            dmGraphics::SetCullFace(m_GraphicsContext, (dmGraphics::FaceType) m_DefoldPipelineState.m_CullFaceType);
            dmGraphics::SetBlendFunc(m_GraphicsContext, (dmGraphics::BlendFactor) m_DefoldPipelineState.m_BlendSrcFactor, (dmGraphics::BlendFactor) m_DefoldPipelineState.m_BlendDstFactor);

            dmGraphics::SetStencilMask(m_GraphicsContext, m_DefoldPipelineState.m_StencilWriteMask);

            dmGraphics::SetStencilFuncSeparate(m_GraphicsContext, dmGraphics::FACE_TYPE_FRONT, (dmGraphics::CompareFunc) m_DefoldPipelineState.m_StencilFrontTestFunc, m_DefoldPipelineState.m_StencilReference, m_DefoldPipelineState.m_StencilCompareMask);
            dmGraphics::SetStencilFuncSeparate(m_GraphicsContext, dmGraphics::FACE_TYPE_BACK, (dmGraphics::CompareFunc) m_DefoldPipelineState.m_StencilBackTestFunc, m_DefoldPipelineState.m_StencilReference, m_DefoldPipelineState.m_StencilCompareMask);

            dmGraphics::SetStencilOpSeparate(m_GraphicsContext, dmGraphics::FACE_TYPE_FRONT,
                (dmGraphics::StencilOp) m_DefoldPipelineState.m_StencilFrontOpFail,
                (dmGraphics::StencilOp) m_DefoldPipelineState.m_StencilFrontOpDepthFail,
                (dmGraphics::StencilOp) m_DefoldPipelineState.m_StencilFrontOpPass);

            dmGraphics::SetStencilOpSeparate(m_GraphicsContext, dmGraphics::FACE_TYPE_BACK,
                (dmGraphics::StencilOp) m_DefoldPipelineState.m_StencilBackOpFail,
                (dmGraphics::StencilOp) m_DefoldPipelineState.m_StencilBackOpDepthFail,
                (dmGraphics::StencilOp) m_DefoldPipelineState.m_StencilBackOpPass);

            dmGraphics::SetColorMask(m_GraphicsContext,
                m_DefoldPipelineState.m_WriteColorMask & (1<<3),
                m_DefoldPipelineState.m_WriteColorMask & (1<<2),
                m_DefoldPipelineState.m_WriteColorMask & (1<<1),
                m_DefoldPipelineState.m_WriteColorMask & (1<<0));
        }

        void OnSizeChanged(uint32_t width, uint32_t height, uint32_t sample_count, bool do_final_blit) override
        {
            uint32_t fbo_id = GetFrameBufferId(width, height, do_final_blit);
            uint32_t fbo_samples = do_final_blit ? 0 : sample_count;

            m_RenderTarget = rive::make_rcp<rive::gpu::FramebufferRenderTargetGL>(width, height, fbo_id, fbo_samples);
            OpenGLCheckError("OnSizeChanged After");

            glViewport(0, 0, width, height);
        }

        void SetGraphicsContext(dmGraphics::HContext graphics_context) override
        {
            m_GraphicsContext = graphics_context;
        }

        void SetRenderTargetTexture(dmGraphics::HTexture texture) override
        {

        }

        dmGraphics::HTexture GetBackingTexture() override
        {
            return dmGraphics::GetRenderTargetTexture(m_DefoldRenderTarget, dmGraphics::BUFFER_TYPE_COLOR0_BIT);
        }

        rive::rcp<rive::gpu::Texture> MakeImageTexture(uint32_t width,
                                                      uint32_t height,
                                                      uint32_t mipLevelCount,
                                                      const uint8_t imageDataRGBA[]) override
        {
            if (mipLevelCount < 1)
                mipLevelCount = 1;

            auto renderContextImpl = m_RenderContext->static_impl_cast<rive::gpu::RenderContextGLImpl>();
            auto texture = renderContextImpl->makeImageTexture(width, height, mipLevelCount, imageDataRGBA);
            OpenGLCheckError("MakeImageTexture After");
            return texture;
        }

    private:

        void SetDefoldGraphicsState(dmGraphics::State state, bool flag)
        {
            if (flag)
                dmGraphics::EnableState(m_GraphicsContext, state);
            else
                dmGraphics::DisableState(m_GraphicsContext, state);
        }

        uint32_t GetFrameBufferId(uint32_t width, uint32_t height, bool do_final_blit)
        {
            // Use framebuffer for the final blit
            if (!do_final_blit)
                return 0;

            // Otherwise, use an intermediate framebuffer for the final blit
            if (!m_DefoldRenderTarget)
            {
                dmGraphics::RenderTargetCreationParams params = {};

                params.m_ColorBufferCreationParams[0].m_Type           = dmGraphics::TEXTURE_TYPE_2D;
                params.m_ColorBufferCreationParams[0].m_Width          = width;
                params.m_ColorBufferCreationParams[0].m_Height         = height;
                params.m_ColorBufferCreationParams[0].m_OriginalWidth  = width;
                params.m_ColorBufferCreationParams[0].m_OriginalHeight = height;
                params.m_ColorBufferCreationParams[0].m_MipMapCount    = 1;

                params.m_ColorBufferParams[0].m_Data     = 0;
                params.m_ColorBufferParams[0].m_DataSize = 0;
                params.m_ColorBufferParams[0].m_Format   = dmGraphics::TEXTURE_FORMAT_RGBA;
                params.m_ColorBufferParams[0].m_Width    = width;
                params.m_ColorBufferParams[0].m_Height   = height;
                params.m_ColorBufferParams[0].m_Depth    = 1;

                params.m_DepthBufferCreationParams.m_Type           = dmGraphics::TEXTURE_TYPE_2D;
                params.m_DepthBufferCreationParams.m_Width          = width;
                params.m_DepthBufferCreationParams.m_Height         = height;
                params.m_DepthBufferCreationParams.m_OriginalWidth  = width;
                params.m_DepthBufferCreationParams.m_OriginalHeight = height;
                params.m_DepthBufferCreationParams.m_MipMapCount    = 1;

                params.m_DepthBufferParams.m_Data     = 0;
                params.m_DepthBufferParams.m_DataSize = 0;
                params.m_DepthBufferParams.m_Format   = dmGraphics::TEXTURE_FORMAT_DEPTH;
                params.m_DepthBufferParams.m_Width    = width;
                params.m_DepthBufferParams.m_Height   = height;
                params.m_DepthBufferParams.m_Depth    = 1;

                params.m_StencilBufferCreationParams.m_Type           = dmGraphics::TEXTURE_TYPE_2D;
                params.m_StencilBufferCreationParams.m_Width          = width;
                params.m_StencilBufferCreationParams.m_Height         = height;
                params.m_StencilBufferCreationParams.m_OriginalWidth  = width;
                params.m_StencilBufferCreationParams.m_OriginalHeight = height;
                params.m_StencilBufferCreationParams.m_MipMapCount    = 1;

                params.m_StencilBufferParams.m_Data     = 0;
                params.m_StencilBufferParams.m_DataSize = 0;
                params.m_StencilBufferParams.m_Format   = dmGraphics::TEXTURE_FORMAT_STENCIL;
                params.m_StencilBufferParams.m_Width    = width;
                params.m_StencilBufferParams.m_Height   = height;
                params.m_StencilBufferParams.m_Depth    = 1;

                uint32_t buffer_flags = dmGraphics::BUFFER_TYPE_COLOR0_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT | dmGraphics::BUFFER_TYPE_STENCIL_BIT;
                m_DefoldRenderTarget  = dmGraphics::NewRenderTarget(m_GraphicsContext, buffer_flags, params);
            }
            else
            {
                dmGraphics::SetRenderTargetSize(m_DefoldRenderTarget, width, height);
            }
            assert(m_DefoldRenderTarget);
            return dmGraphics::OpenGLGetRenderTargetId(m_GraphicsContext, m_DefoldRenderTarget);
        }

        std::unique_ptr<rive::gpu::RenderContext> m_RenderContext;
        dmGraphics::HContext                      m_GraphicsContext;
        rive::rcp<rive::gpu::RenderTargetGL>      m_RenderTarget;
        dmGraphics::PipelineState                 m_DefoldPipelineState;
        dmGraphics::HRenderTarget                 m_DefoldRenderTarget;
    };

    IDefoldRiveRenderer* MakeDefoldRiveRendererOpenGL()
    {
        return new DefoldRiveRendererOpenGL();
    }
}

#endif
