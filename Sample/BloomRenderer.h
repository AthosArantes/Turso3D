#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Math/IntVector2.h>
#include <Turso3D/Math/Vector2.h>
#include <memory>

namespace Turso3D
{
	class FrameBuffer;
	class Graphics;
	class ShaderProgram;
	class Texture;

	class BlurRenderer;

	class BloomRenderer
	{
	public:
		BloomRenderer();
		~BloomRenderer();

		void Initialize(Graphics* graphics);

		void UpdateBuffers(const IntVector2& size, ImageFormat format);
		void Render(Texture* hdrColor, float brightThreshold = 3.0f, float intensity = 0.05f);

		FrameBuffer* GetResultFramebuffer() const { return resultFbo.get(); }
		Texture* GetResultTexture() const { return resultTexture.get(); }

	private:
		// Cached graphics subsystem
		Graphics* graphics;

		std::shared_ptr<ShaderProgram> programBrightness;
		int uBrightThreshold;

		std::shared_ptr<ShaderProgram> programCompose;
		int uIntensity;

		std::unique_ptr<BlurRenderer> blurRenderer;

		std::unique_ptr<Texture> resultTexture;
		std::unique_ptr<FrameBuffer> resultFbo;
	};
}
