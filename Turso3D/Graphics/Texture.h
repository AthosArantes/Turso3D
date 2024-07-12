#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Math/Color.h>
#include <Turso3D/Math/IntBox.h>
#include <Turso3D/Math/IntRect.h>
#include <Turso3D/Resource/Resource.h>
#include <vector>
#include <memory>

namespace Turso3D
{
	class Stream;

	// Description of image mip level data.
	struct ImageLevel
	{
		// Pointer to pixel data.
		const void* data;
		// Total data size in bytes.
		// Optional for non-compressed data.
		int dataSize;

		// Dimensions in pixels.
		IntBox dimensions;

		int layer_face;
		int level;
	};

	// Texture on the GPU.
	class Texture : public Resource
	{
		struct LoadBuffer;

	public:
		enum LoadFlag
		{
			LOAD_FLAG_SRGB = 0x1,
			LOAD_FLAG_GENERATE_MIPS = 0x2
		};

	public:
		// Construct.
		Texture();
		// Construct.
		Texture(unsigned loadFlags);
		// Destruct.
		~Texture();

		// Load the texture image data from a stream.
		// Return true on success.
		bool BeginLoad(Stream& source) override;
		// Finish texture loading by uploading to the GPU.
		// Return true on success.
		bool EndLoad() override;

		// Allocate a texture on the GPU.
		// The size determines the dimensions of the texture, but for some texture types each component have it's own meaning.
		// e.g:
		//	TARGET_1D: [x] is the texture width, [y,z] are ignored.
		//	TARGET_1D_ARRAY: [x] is the texture width, [y] is the number of layers, [z] is ignored.
		//	TARGET_2D: [x,y] are the texture width and height, [z] is ignored.
		//	TARGET_2D_ARRAY: [x,y] are the texture width and height, [z] is the number of layers.
		//	TARGET_CUBE: [x,y] are the texture width and height, [z] must be MAX_CUBE_FACES.
		//	TARGET_CUBE_ARRAY: [x,y] are the texture width and height, [z] must be the number of cubemaps * MAX_CUBE_FACES.
		//	TARGET_3D: [x,y,z] are the texture width, height and depth.
		// Returns true on success.
		bool Define(TextureTarget type, const IntVector3& size, ImageFormat format, int multisample = 1, int numLevels = 1);
		bool Define(TextureTarget type, const IntVector2& size, ImageFormat format, int multisample = 1, int numLevels = 1)
		{
			return Define(type, IntVector3 {size.x, size.y, 1}, format, multisample, numLevels);
		}
		// Define sampling parameters.
		// Return true on success.
		bool DefineSampler(TextureFilterMode filter = FILTER_ANISOTROPIC, TextureAddressMode u = ADDRESS_WRAP, TextureAddressMode v = ADDRESS_WRAP, TextureAddressMode w = ADDRESS_WRAP, unsigned maxAnisotropy = 16, float minLod = -M_MAX_FLOAT, float maxLod = M_MAX_FLOAT, const Color& borderColor = Color::BLACK());

		// Upload texture data to GPU.
		// Return true on success.
		bool SetData(const ImageLevel& data);

		// Return texture target type.
		TextureTarget Target() const { return type; }
		// Return dimensions in pixels.
		const IntVector3& Size() const { return size; }
		// Return 2D dimensions in pixels.
		IntVector2 Size2D() const { return IntVector2 {size.x, size.y}; }

		// Return image format.
		ImageFormat Format() const { return format; }

		// Return multisampling level, or 1 if not multisampled.
		int Multisample() const { return multisample; }
		// Return number of mipmap levels.
		int NumLevels() const { return numLevels; }

		// Return texture filter mode.
		TextureFilterMode FilterMode() const { return filter; }
		// Return texture addressing mode by index.
		TextureAddressMode AddressMode(size_t index) const { return addressModes[index]; }
		// Return max anisotropy.
		unsigned MaxAnisotropy() const { return maxAnisotropy; }
		// Return minimum LOD.
		float MinLod() const { return minLod; }
		// Return maximum LOD.
		float MaxLod() const { return maxLod; }
		// Return border color.
		const Color& BorderColor() const { return borderColor; }

		// Set loading flags.
		// Must be set before calling BeginLoad().
		void SetLoadFlag(unsigned flag, bool set)
		{
			if (set) {
				loadFlags |= flag;
			} else {
				loadFlags &= ~flag;
			}
		}
		// Return the load flags.
		unsigned LoadFlags() const { return loadFlags; }

		// Return the OpenGL object identifier.
		unsigned GLTexture() const { return texture; }
		// Return the OpenGL binding target of the texture.
		unsigned GLTarget() const { return target; }

		// OpenGL texture internal formats by image format.
		static unsigned GetGLInternalFormat(ImageFormat format);
		// Return whether a format is compressed or not.
		static bool IsCompressed(ImageFormat format);
		// Return whether the format has stencil component.
		static bool IsStencil(ImageFormat format);
		// Return the number of bits per pixel.
		static size_t BitsPerPixel(ImageFormat format);

	private:
		// Release the texture.
		void Release();

	private:
		// OpenGL object identifier.
		unsigned texture;
		// OpenGL texture target.
		unsigned target;

		// Texture target type.
		TextureTarget type;
		// Texture dimensions in pixels.
		IntVector3 size;
		// Image format.
		ImageFormat format;
		// Multisampling level.
		int multisample;
		// Number of mipmap levels.
		int numLevels;

		// Texture filtering mode.
		TextureFilterMode filter;
		// Texture addressing modes for each coordinate axis.
		TextureAddressMode addressModes[3];
		// Maximum anisotropy.
		unsigned maxAnisotropy;
		// Minimum LOD.
		float minLod;
		// Maximum LOD.
		float maxLod;
		// Border color.
		// Only effective in border addressing mode.
		Color borderColor;

		// Loading flags
		unsigned loadFlags;

		// Loading buffer used to hold temporary image data. Internal use only.
		std::unique_ptr<LoadBuffer> loadBuffer;
	};
}
