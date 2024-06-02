#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <memory>

namespace Turso3D
{
	class FrameBuffer;
	class Graphics;
	class ShaderProgram;
	class Texture;
	class IntVector2;
	class BlurRenderer;

	class BloomRenderer
	{
	public:
		BloomRenderer();
		~BloomRenderer();

		void Initialize(Graphics* graphics);

		void UpdateBuffers(const IntVector2& size, ImageFormat format);
		void Render(Texture* hdrColor, float intensity = 0.05f);

		FrameBuffer* GetFramebuffer() const { return fbo.get(); }
		Texture* GetTexture() const { return buffer.get(); }

	private:
		// Cached graphics subsystem.
		Graphics* graphics;

		std::unique_ptr<BlurRenderer> blurRenderer;

		std::shared_ptr<ShaderProgram> bloomProgram;
		int uIntensity; // intensity uniform location.

		std::unique_ptr<Texture> buffer;
		std::unique_ptr<FrameBuffer> fbo;
	};
}
