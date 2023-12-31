#include <Turso3D/Math/IntVector2.h>
#include <Turso3D/Math/Vector2.h>
#include <Turso3D/Resource/Image.h>
#include <memory>
#include <vector>

namespace Turso3D
{
	class FrameBuffer;
	class Graphics;
	class ShaderProgram;
	class Texture;

	class BlurRenderer
	{
	public:
		BlurRenderer();
		~BlurRenderer();

		void Initialize(Graphics* graphics);

		void UpdateBuffers(const IntVector2& size, size_t maxMips = 0, ImageFormat format = FMT_RGBA8, bool srgb = false);
		void Render(Texture* color, float filterRadius = 0.005f);

		FrameBuffer* GetResultFramebuffer() const { return resultFbo.get(); }
		Texture* GetResultTexture() const { return resultTexture.get(); }

	private:
		// Cached graphics subsystem
		Graphics* graphics;

		std::shared_ptr<ShaderProgram> programDownsample;
		int uInvSrcSize;

		std::shared_ptr<ShaderProgram> programUpsample;
		int uFilterRadius;

		struct MipPass
		{
			std::unique_ptr<Texture> texture;
			std::unique_ptr<FrameBuffer> fbo;
		};
		std::vector<MipPass> mipPasses;

		std::unique_ptr<Texture> resultTexture;
		std::unique_ptr<FrameBuffer> resultFbo;
	};
}
