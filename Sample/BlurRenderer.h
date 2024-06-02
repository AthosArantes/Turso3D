#pragma once

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

	class BlurRenderer
	{
		struct MipPass;

	public:
		BlurRenderer();
		~BlurRenderer();

		void Initialize(Graphics* graphics);

		// Perform downsample.
		void Downsample(Texture* srcColor);
		// Perform upsample up to the first mip.
		void Upsample(float filterRadius = 0.005f);

		// Update internal buffers.
		//	size: The texture dimensions for the first mip.
		//	minMipSize: The minimum required mip dimensions. If it's zero then a 1x1 is used.
		//	maxMips: The max number of mip textures to be created. If it's zero then mip textures will be created up to minMipSize.
		void UpdateBuffers(const IntVector2& size, ImageFormat format, const IntVector2& minMipSize, int maxMips = 0);

		// Return the first mip framebuffer.
		FrameBuffer* GetFramebuffer() const;
		// Return the first mip buffer.
		Texture* GetTexture() const;

	private:
		// Cached graphics subsystem
		Graphics* graphics;

		// Mip buffers.
		std::vector<MipPass> passes;

		std::shared_ptr<ShaderProgram> downsampleProgram[2];
		int uInvSrcSize[2];

		std::shared_ptr<ShaderProgram> upsampleProgram;
		int uFilterRadius;
		int uAspectRatio;

		// Cached aspect ratio of the mips.
		float aspectRatio;
	};
}
