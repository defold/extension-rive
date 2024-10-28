
#include <rive/factory.hpp>
#include <rive/renderer.hpp>
#include <rive/renderer/rive_renderer.hpp>
#include <rive/renderer/render_context.hpp>

#include <dmsdk/graphics/graphics.h>

namespace dmRive
{
	class IDefoldRiveRenderer
	{
	public:
		virtual ~IDefoldRiveRenderer() {}
		virtual rive::Factory* Factory() = 0;
		virtual rive::Renderer* MakeRenderer() = 0;
		virtual void OnSizeChanged(uint32_t width, uint32_t height, uint32_t sample_count) = 0;
		virtual void BeginFrame(const rive::gpu::RenderContext::FrameDescriptor& frameDescriptor) = 0;
		virtual void Flush() = 0;
		virtual void SetRenderTargetTexture(dmGraphics::HTexture texture) = 0;
		virtual void SetGraphicsContext(dmGraphics::HContext graphics_context) = 0;

		virtual dmGraphics::HTexture GetBackingTexture() = 0;
		virtual rive::rcp<rive::gpu::Texture> MakeImageTexture(uint32_t width, uint32_t height, uint32_t mipLevelCount, const uint8_t imageDataRGBA[]) = 0;
	};

	IDefoldRiveRenderer* MakeDefoldRiveRendererMetal();
	IDefoldRiveRenderer* MakeDefoldRiveRendererOpenGL();
	IDefoldRiveRenderer* MakeDefoldRiveRendererWebGPU();
}
