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
	public:
		struct MipPass
		{
			std::unique_ptr<Texture> buffer;
			std::unique_ptr<FrameBuffer> fbo;
		};

	public:
		BlurRenderer();
		~BlurRenderer();

		void Initialize(Graphics* graphics);

		void Render(std::vector<MipPass>& passes, Texture* srcColor, float filterRadius = 0.005f);

		// Create mip passes for blurring.
		//	size: The texture dimensions for the first mip.
		//	minMipSize: The minimum required mip dimensions. If it's zero then a 1x1 is used.
		//	maxMips: The max number of mip textures to be created. If it's zero then mip textures will be created up to minMipSize.
		static void CreateMips(std::vector<MipPass>& passes, const IntVector2& baseSize, ImageFormat format, const IntVector2& minMipSize, size_t maxMips = 0);

	private:
		// Cached graphics subsystem
		Graphics* graphics;

		std::shared_ptr<ShaderProgram> downsampleProgram;
		int uInvSrcSize;

		std::shared_ptr<ShaderProgram> upsampleProgram;
		int uFilterRadius;
	};
}
