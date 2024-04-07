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
		// Default construct.
		ImageLevel();
		// Construct with parameters for non-compressed data.
		ImageLevel(const IntVector2& size, ImageFormat format, const void* data);
		// Construct with parameters for non-compressed data.
		ImageLevel(const IntVector3& size, ImageFormat format, const void* data);

		// Pointer to pixel data.
		const uint8_t* data;
		// Level size in pixels.
		IntVector3 size;
		// Total data size in bytes.
		size_t dataSize;
	};

	// Texture on the GPU.
	class Texture : public Resource
	{
		struct LoadBuffer;

	public:
		// Construct.
		Texture();
		// Construct.
		Texture(bool loadSRGB);
		// Destruct.
		~Texture();

		// Load the texture image data from a stream.
		// Return true on success.
		bool BeginLoad(Stream& source) override;
		// Finish texture loading by uploading to the GPU.
		// Return true on success.
		bool EndLoad() override;

		// Define texture type and dimensions and set initial data.
		// Return true on success.
		bool Define(TextureType type, const IntVector2& size, ImageFormat format, int multisample = 1, size_t numLevels = 1, const ImageLevel* initialData = nullptr);
		// Define texture type and dimensions and set initial data.
		// Return true on success.
		bool Define(TextureType type, const IntVector3& size, ImageFormat format, int multisample = 1, size_t numLevels = 1, const ImageLevel* initialData = nullptr);

		// Define sampling parameters.
		// Return true on success.
		bool DefineSampler(TextureFilterMode filter = FILTER_ANISOTROPIC, TextureAddressMode u = ADDRESS_WRAP, TextureAddressMode v = ADDRESS_WRAP, TextureAddressMode w = ADDRESS_WRAP, unsigned maxAnisotropy = 16, float minLod = -M_MAX_FLOAT, float maxLod = M_MAX_FLOAT, const Color& borderColor = Color::BLACK());

		// Set data for a mipmap level.
		// Return true on success.
		bool SetData(size_t level, const IntRect& rect, const ImageLevel& data);
		// Set data for a mipmap level.
		// Return true on success.
		bool SetData(size_t level, const IntBox& box, const ImageLevel& data);

		// Bind to texture unit.
		// No-op if already bound.
		void Bind(size_t unit);

		// Return texture type.
		TextureType TexType() const { return type; }
		// Return dimensions in pixels.
		const IntVector3& Size() const { return size; }
		// Return 2D dimensions in pixels.
		IntVector2 Size2D() const { return IntVector2 {size.x, size.y}; }

		// Return width in pixels.
		int Width() const { return size.x; }
		// Return height in pixels.
		int Height() const { return size.y; }
		// Return depth in pixels.
		// For cube maps, returns the number of faces.
		int Depth() const { return size.z; }

		// Return image format.
		ImageFormat Format() const { return format; }

		// Return multisampling level, or 1 if not multisampled.
		int Multisample() const { return multisample; }
		// Return number of mipmap levels.
		size_t NumLevels() const { return numLevels; }

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

		// Set the sRGB flag used when loading from a stream.
		// Must be set before calling EndLoad().
		void SetLoadSRGB(bool srgb = true) { loadSRGB = srgb; }
		// Return whether the image will be loaded in sRGB format.
		bool GetLoadSRGB() const { return loadSRGB; }

		// Return the OpenGL object identifier.
		unsigned GLTexture() const { return texture; }
		// Return the OpenGL binding target of the texture.
		unsigned GLTarget() const;

		// Unbind a texture unit.
		static void Unbind(size_t unit);

		// OpenGL texture internal formats by image format.
		static unsigned GetGLInternalFormat(ImageFormat format);
		// Return whether a format is compressed or not.
		static bool IsCompressed(ImageFormat format);
		// Return whether the format has stencil component.
		static bool IsStencil(ImageFormat format);

	private:
		// Force bind to the first texture unit. Used when editing.
		void ForceBind();
		// Release the texture.
		void Release();

	private:
		// OpenGL object identifier.
		unsigned texture;
		// OpenGL texture target.
		unsigned target;

		// Texture type.
		TextureType type;
		// Texture dimensions in pixels.
		IntVector3 size;
		// Image format.
		ImageFormat format;
		// Multisampling level.
		int multisample;
		// Number of mipmap levels.
		size_t numLevels;

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

		// sRGB load flag. Must be set before loading from a stream.
		bool loadSRGB;

		// Loading buffer used to hold temporary image data. Internal use only.
		std::unique_ptr<LoadBuffer> loadBuffer;
	};
}
