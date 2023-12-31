#include <Turso3D/Math/IntVector2.h>
#include <Turso3D/Math/IntRect.h>
#include <Turso3D/Math/Vector2.h>
#include <memory>
#include <vector>

namespace Turso3D
{
	class Camera;
	class FrameBuffer;
	class Graphics;
	class ShaderProgram;
	class Texture;

	class SSAORenderer
	{
	public:
		SSAORenderer();
		~SSAORenderer();

		void Initialize(Graphics* graphics);

		void UpdateBuffers(const IntVector2& size);
		void Render(Camera* camera, Texture* normal, Texture* depth, FrameBuffer* dst, const IntRect& viewRect);

		FrameBuffer* GetResultFramebuffer() const { return resultFbo.get(); }
		Texture* GetResultTexture() const { return resultTexture.get(); }

	private:
		void GenerateNoiseTexture();

	private:
		// Cached graphics subsystem
		Graphics* graphics;

		std::shared_ptr<ShaderProgram> program;
		int uNoiseInvSize;
		int uAoParameters;
		int uScreenInvSize;
		int uFrustumSize;
		int uDepthReconstruct;

		std::shared_ptr<ShaderProgram> programBlur;
		int uBlurInvSize;

		std::unique_ptr<Texture> noiseTexture;
		std::unique_ptr<Texture> resultTexture;
		std::unique_ptr<FrameBuffer> resultFbo;
	};
}
