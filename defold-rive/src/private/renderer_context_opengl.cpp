
#ifdef DM_RIVE_USE_OPENGL

// FIXME
#include <GLES3/gl3.h>

#include <dmsdk/graphics/graphics_native.h>
#include <dmsdk/dlib/log.h>

#include "renderer_context.h"

#define RIVE_ANDROID
#include <rive/renderer/rive_renderer.hpp>
#include <rive/renderer/texture.hpp>
#include <rive/renderer/gl/render_context_gl_impl.hpp>
#include <rive/renderer/gl/render_target_gl.hpp>

#include <private/defold_graphics.h>

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
            auto renderContextImpl = m_RenderContext->static_impl_cast<rive::gpu::RenderContextGLImpl>();
            auto texture = renderContextImpl->makeImageTexture(width, height, mipLevelCount, imageDataRGBA);
            dmLogInfo("MakeImageTexture %d, %d, %d", width, height, mipLevelCount);
            OpenGLClearGLError("MakeImageTexture After");
            return texture;
        }

    private:
        std::unique_ptr<rive::gpu::RenderContext> m_RenderContext;
        dmGraphics::HContext                      m_GraphicsContext;
        rive::rcp<rive::gpu::RenderTargetGL>      m_RenderTarget;
    };

    IDefoldRiveRenderer* MakeDefoldRiveRendererOpenGL()
    {
        return new DefoldRiveRendererOpenGL();
    }
}

#endif
