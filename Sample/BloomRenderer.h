#pragma once

#include "BlurRenderer.h"
#include <Turso3D/Graphics/GraphicsDefs.h>
#include <memory>
#include <vector>

namespace Turso3D
{
	class FrameBuffer;
	class Graphics;
	class ShaderProgram;
	class Texture;
	class IntVector2;

	class BloomRenderer
	{
	public:
		BloomRenderer();
		~BloomRenderer();

		void Initialize(Graphics* graphics);

		void UpdateBuffers(const IntVector2& size, ImageFormat format);
		void Render(BlurRenderer* blurRenderer, Texture* hdrColor, float intensity = 0.05f);

		FrameBuffer* GetResultFramebuffer() const { return resultFbo.get(); }
		Texture* GetResultTexture() const { return resultTexture.get(); }

	private:
		// Cached graphics subsystem.
		Graphics* graphics;

		std::vector<BlurRenderer::MipPass> blurPasses;

		std::shared_ptr<ShaderProgram> bloomProgram;
		int uIntensity; // intensity uniform location.

		std::unique_ptr<Texture> resultTexture;
		std::unique_ptr<FrameBuffer> resultFbo;
	};
}
