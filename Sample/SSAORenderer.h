#include <Turso3D/Math/IntVector2.h>
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
		// Constructor
		// Graphics subsystem must have been initialized.
		SSAORenderer(Graphics* graphics);
		// Destructor
		~SSAORenderer();

		void UpdateBuffers(const IntVector2& size);
		void Render(Camera* camera, Texture* normal, Texture* depth, FrameBuffer* dst);

		Texture* GetTexture() const { return texBuffer.get(); }

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

		std::shared_ptr<ShaderProgram> blurProgram;
		int uBlurInvSize;

		std::unique_ptr<Texture> texNoise;
		std::unique_ptr<Texture> texBuffer;
		std::unique_ptr<FrameBuffer> fbo;

		IntVector2 screenSize;
		Vector2 invScreenSize;

		Vector2 invTexSize;
	};
}
