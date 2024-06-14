#pragma once

#include <Turso3D/Math/Vector4.h>
#include <Turso3D/fwd.h>
#include <memory>

class SSAORenderer
{
	struct UniformDataBlock
	{
		alignas(8) Turso3D::Vector2 noiseInvSize;
		alignas(8) Turso3D::Vector2 screenInvSize;

		alignas(16) Turso3D::Vector4 frustumSize;
		alignas(8) Turso3D::Vector2 depthReconstruct;

		alignas(16) Turso3D::Vector4 aoParams;
	};

public:
	SSAORenderer();
	~SSAORenderer();

	void Initialize(Turso3D::Graphics* graphics);

	void UpdateBuffers(const Turso3D::IntVector2& size);
	void Render(Turso3D::Camera* camera, Turso3D::Texture* normal, Turso3D::Texture* depth, Turso3D::FrameBuffer* dst, const Turso3D::IntRect& viewRect);

	Turso3D::FrameBuffer* GetResultFramebuffer() const { return resultFbo.get(); }
	Turso3D::Texture* GetResultTexture() const { return resultTexture.get(); }

private:
	void GenerateNoiseTexture();

private:
	// Cached graphics subsystem
	Turso3D::Graphics* graphics;

	std::shared_ptr<Turso3D::ShaderProgram> ssaoProgram;

	UniformDataBlock uniformData;
	std::unique_ptr<Turso3D::UniformBuffer> ssaoUniformBuffer;
	bool uniformDataDirty;

	std::shared_ptr<Turso3D::ShaderProgram> blurProgram;
	int uBlurInvSize;

	std::unique_ptr<Turso3D::Texture> noiseTexture;
	std::unique_ptr<Turso3D::Texture> resultTexture;
	std::unique_ptr<Turso3D::FrameBuffer> resultFbo;
};
