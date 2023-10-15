#pragma once

#include <Turso3D/Resource/Image.h>

namespace Turso3D
{
	// Decompress DXT1/3/5 image data.
	void DecompressImageDXT(unsigned char* dest, const void* blocks, int width, int height, ImageFormat format);
	// Decompress ETC image data.
	void DecompressImageETC(unsigned char* dest, const void* blocks, int width, int height);
	// Decompress PVRTC image data.
	void DecompressImagePVRTC(unsigned char* dest, const void* blocks, int width, int height, ImageFormat format);
}
