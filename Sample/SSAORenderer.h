#pragma once

#include <Turso3D/Math/IntVector2.h>
#include <Turso3D/Math/IntRect.h>
#include <Turso3D/Math/Vector4.h>
#include <memory>
#include <vector>

namespace Turso3D
{
	class Camera;
	class FrameBuffer;
	class Graphics;
	class ShaderProgram;
	class UniformBuffer;
	class Texture;

	class SSAORenderer
	{
		struct UniformDataBlock
		{
			alignas(8) Vector2 noiseInvSize;
			alignas(8) Vector2 screenInvSize;

			alignas(16) Vector4 frustumSize;
			alignas(8) Vector2 depthReconstruct;

			alignas(16) Vector4 aoParams;
		};

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

		std::shared_ptr<ShaderProgram> ssaoProgram;

		std::unique_ptr<UniformBuffer> ssaoUniformBuffer;
		UniformDataBlock uniformData;
		bool uniformDataDirty;

		std::shared_ptr<ShaderProgram> blurProgram;
		int uBlurInvSize;

		std::unique_ptr<Texture> noiseTexture;
		std::unique_ptr<Texture> resultTexture;
		std::unique_ptr<FrameBuffer> resultFbo;
	};
}
