
#ifdef DM_RIVE_USE_OPENGL

#if defined(RIVE_ANDROID) || defined(RIVE_WEBGL)
    #undef GL_ES_VERSION_2_0
    #undef GL_ES_VERSION_3_0

    #include <GLES3/gl3.h>
#endif

#include <dmsdk/graphics/graphics_native.h>
#include <dmsdk/dlib/log.h>

#include "renderer_context.h"

#include <rive/renderer/rive_renderer.hpp>
#include <rive/renderer/texture.hpp>
#include <rive/renderer/gl/render_context_gl_impl.hpp>
#include <rive/renderer/gl/render_target_gl.hpp>

#include <private/defold_graphics.h>

#ifdef RIVE_DESKTOP_GL
    #define GLFW_INCLUDE_NONE
    #include "GLFW/glfw3.h"
#endif

static void OpenGLClearGLError(const char* context)
{
    GLint err = glGetError();
    while (err != 0)
    {
        dmLogInfo("%s - OpenGL Error %d", context, err);
        err = glGetError();
    }
}

namespace dmRive
{
    class DefoldRiveRendererOpenGL : public IDefoldRiveRenderer
    {
    public:
        DefoldRiveRendererOpenGL()
        {
        #ifdef RIVE_DESKTOP_GL
            // Load the OpenGL API using glad.
            if (!gladLoadCustomLoader((GLADloadproc)glfwGetProcAddress))
            {
                fprintf(stderr, "Failed to initialize glad.\n");
                abort();
            }
        #endif

            dmLogInfo("==== GL GPU: %s ====\n", glGetString(GL_RENDERER)); 

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
            OpenGLClearGLError("BeginFrame After");
        }

        void Flush() override
        {
            m_RenderContext->flush({.renderTarget = m_RenderTarget.get()});
            m_RenderContext->static_impl_cast<rive::gpu::RenderContextGLImpl>()->unbindGLInternalResources();
            OpenGLClearGLError("Flush After");
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Rive messes up the state after flush it seems.
            SetDefoldGraphicsState(dmGraphics::STATE_CULL_FACE, m_DefoldPipelineState.m_CullFaceEnabled);
            SetDefoldGraphicsState(dmGraphics::STATE_BLEND, m_DefoldPipelineState.m_BlendEnabled);

            dmGraphics::SetCullFace(m_GraphicsContext, (dmGraphics::FaceType) m_DefoldPipelineState.m_CullFaceType);
            dmGraphics::SetBlendFunc(m_GraphicsContext, (dmGraphics::BlendFactor) m_DefoldPipelineState.m_BlendSrcFactor, (dmGraphics::BlendFactor) m_DefoldPipelineState.m_BlendDstFactor);
        }

        void OnSizeChanged(uint32_t width, uint32_t height) override
        {
            m_RenderTarget = rive::make_rcp<rive::gpu::FramebufferRenderTargetGL>(width, height, 0, 4);
            OpenGLClearGLError("OnSizeChanged After");
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
            return 0;
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
            dmLogInfo("MakeImageTexture %d, %d, %d", width, height, mipLevelCount);
            OpenGLClearGLError("MakeImageTexture After");
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

        std::unique_ptr<rive::gpu::RenderContext> m_RenderContext;
        dmGraphics::HContext                      m_GraphicsContext;
        rive::rcp<rive::gpu::RenderTargetGL>      m_RenderTarget;
        dmGraphics::PipelineState                 m_DefoldPipelineState;
    };

    IDefoldRiveRenderer* MakeDefoldRiveRendererOpenGL()
    {
        return new DefoldRiveRendererOpenGL();
    }
}

#endif
