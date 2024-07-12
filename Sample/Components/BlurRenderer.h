#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/fwd.h>
#include <memory>
#include <vector>

class BlurRenderer
{
	struct MipPass;

public:
	BlurRenderer();
	~BlurRenderer();

	void Initialize();

	// Perform downsample.
	void Downsample(Turso3D::Texture* srcColor);
	// Perform upsample up to the first mip.
	void Upsample(float filterRadius = 0.005f);

	// Update internal buffers.
	//	size: The texture dimensions for the first mip.
	//	minMipSize: The minimum required mip dimensions. If it's zero then a 1x1 is used.
	//	maxMips: The max number of mip textures to be created. If it's zero then mip textures will be created up to minMipSize.
	void UpdateBuffers(const Turso3D::IntVector2& size, Turso3D::ImageFormat format, const Turso3D::IntVector2& minMipSize, int maxMips = 0);

	// Return the first mip framebuffer.
	Turso3D::FrameBuffer* GetFramebuffer() const;
	// Return the first mip buffer.
	Turso3D::Texture* GetTexture() const;

private:
	// Mip buffers.
	std::vector<MipPass> passes;

	std::shared_ptr<Turso3D::ShaderProgram> downsampleProgram[2];
	int uInvSrcSize[2];

	std::shared_ptr<Turso3D::ShaderProgram> upsampleProgram;
	int uFilterRadius;
	int uAspectRatio;

	// Cached aspect ratio of the mips.
	float aspectRatio;
};
