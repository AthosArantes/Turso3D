#include <Turso3D/Math/IntVector2.h>
#include <Turso3D/Math/Vector2.h>
#include <memory>
#include <vector>

namespace Turso3D
{
	class FrameBuffer;
	class Graphics;
	class RenderBuffer;
	class ShaderProgram;
	class Texture;

	class BloomRenderer
	{
	public:
		// Constructor
		// Graphics subsystem must have been initialized.
		BloomRenderer(Graphics* graphics);
		// Destructor
		~BloomRenderer();

		void SetParameters(float brightThreshold, float filterRadius, float intensity);
		void UpdateBuffers(const IntVector2& size);
		void Render(Texture* hdrColor);

		Texture* GetComposedTexture() const { return texBuffer.get(); }

	private:
		// Cached graphics subsystem
		Graphics* graphics;

		std::shared_ptr<ShaderProgram> brightProgram;
		int uBrightThreshold;
		float brightThreshold;

		std::shared_ptr<ShaderProgram> downsampleProgram;
		int uInvTexSize;

		std::shared_ptr<ShaderProgram> upsampleProgram;
		int uFilterRadius;
		float filterRadius;

		std::shared_ptr<ShaderProgram> composeProgram;
		int uIntensity;
		float intensity;

		std::unique_ptr<Texture> texBuffer;
		std::unique_ptr<FrameBuffer> fbo;

		struct MipPass
		{
			std::unique_ptr<Texture> buffer;
			std::unique_ptr<FrameBuffer> downsampleFbo;
			std::unique_ptr<FrameBuffer> upsampleFbo;
		};
		std::vector<MipPass> mipPasses;
	};
}
