#include "Texture.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/IO/Stream.h>
#include <Turso3D/IO/Log.h>
#include <glew/glew.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <gli/load.hpp>
#include <gli/generate_mipmaps.hpp>

namespace Turso3D
{
	static const GLenum glTargets[] =
	{
		GL_TEXTURE_2D,
		GL_TEXTURE_3D,
		GL_TEXTURE_CUBE_MAP
	};

	static const unsigned glWrapModes[] = {
		GL_REPEAT,
		GL_MIRRORED_REPEAT,
		GL_CLAMP_TO_EDGE,
		GL_CLAMP_TO_BORDER,
		GL_MIRROR_CLAMP_EXT
	};

	static ImageFormat GetSRGBFormat(ImageFormat format)
	{
		switch (format) {
			case FORMAT_R8_UNORM_PACK8:
			case FORMAT_R8_SNORM_PACK8:
			case FORMAT_R8_USCALED_PACK8:
			case FORMAT_R8_SSCALED_PACK8:
			case FORMAT_R8_UINT_PACK8:
			case FORMAT_R8_SINT_PACK8:
				return FORMAT_R8_SRGB_PACK8;

			case FORMAT_RG8_UNORM_PACK8:
			case FORMAT_RG8_SNORM_PACK8:
			case FORMAT_RG8_USCALED_PACK8:
			case FORMAT_RG8_SSCALED_PACK8:
			case FORMAT_RG8_UINT_PACK8:
			case FORMAT_RG8_SINT_PACK8:
				return FORMAT_RG8_SRGB_PACK8;

			case FORMAT_RGB8_UNORM_PACK8:
			case FORMAT_RGB8_SNORM_PACK8:
			case FORMAT_RGB8_USCALED_PACK8:
			case FORMAT_RGB8_SSCALED_PACK8:
			case FORMAT_RGB8_UINT_PACK8:
			case FORMAT_RGB8_SINT_PACK8:
				return FORMAT_RGB8_SRGB_PACK8;

			case FORMAT_BGR8_UNORM_PACK8:
			case FORMAT_BGR8_SNORM_PACK8:
			case FORMAT_BGR8_USCALED_PACK8:
			case FORMAT_BGR8_SSCALED_PACK8:
			case FORMAT_BGR8_UINT_PACK8:
			case FORMAT_BGR8_SINT_PACK8:
				return FORMAT_BGR8_SRGB_PACK8;

			case FORMAT_RGBA8_UNORM_PACK8:
			case FORMAT_RGBA8_SNORM_PACK8:
			case FORMAT_RGBA8_USCALED_PACK8:
			case FORMAT_RGBA8_SSCALED_PACK8:
			case FORMAT_RGBA8_UINT_PACK8:
			case FORMAT_RGBA8_SINT_PACK8:
				return FORMAT_RGBA8_SRGB_PACK8;

			case FORMAT_BGRA8_UNORM_PACK8:
			case FORMAT_BGRA8_SNORM_PACK8:
			case FORMAT_BGRA8_USCALED_PACK8:
			case FORMAT_BGRA8_SSCALED_PACK8:
			case FORMAT_BGRA8_UINT_PACK8:
			case FORMAT_BGRA8_SINT_PACK8:
				return FORMAT_BGRA8_SRGB_PACK8;

			case FORMAT_RGBA8_UNORM_PACK32:
			case FORMAT_RGBA8_SNORM_PACK32:
			case FORMAT_RGBA8_USCALED_PACK32:
			case FORMAT_RGBA8_SSCALED_PACK32:
			case FORMAT_RGBA8_UINT_PACK32:
			case FORMAT_RGBA8_SINT_PACK32:
				return FORMAT_RGBA8_SRGB_PACK32;

			case FORMAT_RGB_DXT1_UNORM_BLOCK8: return FORMAT_RGB_DXT1_SRGB_BLOCK8;
			case FORMAT_RGBA_DXT1_UNORM_BLOCK8: return FORMAT_RGBA_DXT1_SRGB_BLOCK8;
			case FORMAT_RGBA_DXT3_UNORM_BLOCK16: return FORMAT_RGBA_DXT3_SRGB_BLOCK16;
			case FORMAT_RGBA_DXT5_UNORM_BLOCK16: return FORMAT_RGBA_DXT5_SRGB_BLOCK16;
			case FORMAT_RGBA_BP_UNORM_BLOCK16: return FORMAT_RGBA_BP_SRGB_BLOCK16;

			case FORMAT_RGB_ETC2_UNORM_BLOCK8: return FORMAT_RGB_ETC2_SRGB_BLOCK8;
			case FORMAT_RGBA_ETC2_UNORM_BLOCK8: return FORMAT_RGBA_ETC2_SRGB_BLOCK8;
			case FORMAT_RGBA_ETC2_UNORM_BLOCK16: return FORMAT_RGBA_ETC2_SRGB_BLOCK16;

			case FORMAT_RGBA_ASTC_4X4_UNORM_BLOCK16: return FORMAT_RGBA_ASTC_4X4_SRGB_BLOCK16;
			case FORMAT_RGBA_ASTC_5X4_UNORM_BLOCK16: return FORMAT_RGBA_ASTC_5X4_SRGB_BLOCK16;
			case FORMAT_RGBA_ASTC_5X5_UNORM_BLOCK16: return FORMAT_RGBA_ASTC_5X5_SRGB_BLOCK16;
			case FORMAT_RGBA_ASTC_6X5_UNORM_BLOCK16: return FORMAT_RGBA_ASTC_6X5_SRGB_BLOCK16;
			case FORMAT_RGBA_ASTC_6X6_UNORM_BLOCK16: return FORMAT_RGBA_ASTC_6X6_SRGB_BLOCK16;
			case FORMAT_RGBA_ASTC_8X5_UNORM_BLOCK16: return FORMAT_RGBA_ASTC_8X5_SRGB_BLOCK16;
			case FORMAT_RGBA_ASTC_8X6_UNORM_BLOCK16: return FORMAT_RGBA_ASTC_8X6_SRGB_BLOCK16;
			case FORMAT_RGBA_ASTC_8X8_UNORM_BLOCK16: return FORMAT_RGBA_ASTC_8X8_SRGB_BLOCK16;
			case FORMAT_RGBA_ASTC_10X5_UNORM_BLOCK16: return FORMAT_RGBA_ASTC_10X5_SRGB_BLOCK16;
			case FORMAT_RGBA_ASTC_10X6_UNORM_BLOCK16: return FORMAT_RGBA_ASTC_10X6_SRGB_BLOCK16;
			case FORMAT_RGBA_ASTC_10X8_UNORM_BLOCK16: return FORMAT_RGBA_ASTC_10X8_SRGB_BLOCK16;
			case FORMAT_RGBA_ASTC_10X10_UNORM_BLOCK16: return FORMAT_RGBA_ASTC_10X10_SRGB_BLOCK16;
			case FORMAT_RGBA_ASTC_12X10_UNORM_BLOCK16: return FORMAT_RGBA_ASTC_12X10_SRGB_BLOCK16;
			case FORMAT_RGBA_ASTC_12X12_UNORM_BLOCK16: return FORMAT_RGBA_ASTC_12X12_SRGB_BLOCK16;

			case FORMAT_RGB_PVRTC1_8X8_UNORM_BLOCK32: return FORMAT_RGB_PVRTC1_8X8_SRGB_BLOCK32;
			case FORMAT_RGB_PVRTC1_16X8_UNORM_BLOCK32: return FORMAT_RGB_PVRTC1_16X8_SRGB_BLOCK32;
			case FORMAT_RGBA_PVRTC1_8X8_UNORM_BLOCK32: return FORMAT_RGBA_PVRTC1_8X8_SRGB_BLOCK32;
			case FORMAT_RGBA_PVRTC1_16X8_UNORM_BLOCK32: return FORMAT_RGBA_PVRTC1_16X8_SRGB_BLOCK32;
			case FORMAT_RGBA_PVRTC2_4X4_UNORM_BLOCK8: return FORMAT_RGBA_PVRTC2_4X4_SRGB_BLOCK8;
			case FORMAT_RGBA_PVRTC2_8X4_UNORM_BLOCK8: return FORMAT_RGBA_PVRTC2_8X4_SRGB_BLOCK8;

			case FORMAT_BGR8_UNORM_PACK32: return FORMAT_BGR8_SRGB_PACK32;
		}
		return format;
	}

	// ==========================================================================================
	struct Texture::LoadBuffer
	{
		gli::texture texture;
	};

	// ==========================================================================================
	static size_t ActiveTextureUnit = 0xffffffff;
	static unsigned ActiveTargets[MAX_TEXTURE_UNITS];
	static Texture* BoundTextures[MAX_TEXTURE_UNITS];

	// ==========================================================================================
	ImageLevel::ImageLevel() :
		data(nullptr),
		size(IntVector3::ZERO),
		dataSize(0)
	{
	}

	ImageLevel::ImageLevel(const IntVector2& size, ImageFormat format, const void* data) :
		data(reinterpret_cast<const uint8_t*>(data)),
		size(IntVector3 {size.x, size.y, 1})
	{
		gli::format f = static_cast<gli::format>(format);
		dataSize = gli::block_size(f) * size.x * size.y;
	}

	ImageLevel::ImageLevel(const IntVector3& size, ImageFormat format, const void* data) :
		data(reinterpret_cast<const uint8_t*>(data)),
		size(size)
	{
		gli::format f = static_cast<gli::format>(format);
		dataSize = gli::block_size(f) * size.x * size.y * size.y;
	}

	// ==========================================================================================
	unsigned Texture::GetGLInternalFormat(ImageFormat format)
	{
		gli::gl GL {gli::gl::PROFILE_GL33};
		gli::gl::format texFormat = GL.translate(static_cast<gli::format>(format), gli::swizzles {});
		return texFormat.Internal;
	}

	bool Texture::IsCompressed(ImageFormat format)
	{
		gli::format texFormat = static_cast<gli::format>(format);
		return gli::is_compressed(texFormat) || gli::is_s3tc_compressed(texFormat);
	}

	bool Texture::IsStencil(ImageFormat format)
	{
		gli::format texFormat = static_cast<gli::format>(format);
		return gli::is_stencil(texFormat);
	}

	// ==========================================================================================
	Texture::Texture() :
		texture(0),
		target(0),
		type(TEX_2D),
		size(IntVector3::ZERO),
		format(FORMAT_NONE),
		multisample(0),
		numLevels(0),
		loadSRGB(false)
	{
	}

	Texture::Texture(bool loadSRGB) :
		texture(0),
		target(0),
		type(TEX_2D),
		size(IntVector3::ZERO),
		format(FORMAT_NONE),
		multisample(0),
		numLevels(0),
		loadSRGB(loadSRGB)
	{
	}

	Texture::~Texture()
	{
		Release();
	}

	bool Texture::BeginLoad(Stream& source)
	{
		loadBuffer.reset();

		gli::texture texture;
		{
			size_t sz = source.Size();
			std::unique_ptr<char[]> data = std::make_unique<char[]>(sz);
			size_t length = source.Read(data.get(), sz);

			texture = gli::load(data.get(), length);
		}

		if (texture.empty()) {
			LOG_ERROR("Failed to load texture.");
			return false;
		}

		ImageFormat format = static_cast<ImageFormat>(texture.format());
		bool compressed = IsCompressed(format);

		// Generate mip maps
		if (!compressed && texture.levels() == 1) {
			// TODO: generate mipmaps
		}

		loadBuffer = std::make_unique<LoadBuffer>();
		loadBuffer->texture = texture;

		return true;
	}

	bool Texture::EndLoad()
	{
		// Clear gl errors
		glGetError();

		gli::texture& tex = loadBuffer->texture;

		// Override sRGB format
		ImageFormat texFormat = static_cast<ImageFormat>(tex.format());
		if (loadSRGB) {
			texFormat = GetSRGBFormat(texFormat);
		}

		bool compressed = IsCompressed(texFormat);

		gli::gl GL {gli::gl::PROFILE_GL33};
		gli::gl::format glFormat = GL.translate(static_cast<gli::format>(texFormat), tex.swizzles());
		GLenum glTarget = GL.translate(tex.target());

		if (glFormat.Type == gli::gl::TYPE_NONE) {
			glFormat.Type = gli::gl::TYPE_U8;
		}

		GLuint tid = 0;
		glGenTextures(1, &tid);
		glBindTexture(glTarget, tid);
		glTexParameteri(glTarget, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(glTarget, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(tex.levels() - 1));
		glTexParameteri(glTarget, GL_TEXTURE_SWIZZLE_R, glFormat.Swizzles[0]);
		glTexParameteri(glTarget, GL_TEXTURE_SWIZZLE_G, glFormat.Swizzles[1]);
		glTexParameteri(glTarget, GL_TEXTURE_SWIZZLE_B, glFormat.Swizzles[2]);
		glTexParameteri(glTarget, GL_TEXTURE_SWIZZLE_A, glFormat.Swizzles[3]);

		if (glGetError() != GL_NO_ERROR) {
			LOG_ERROR("Failed to create OpenGL texture.");
			return false;
		}

		glm::tvec3<GLsizei> extent {tex.extent()};
		GLsizei face_count = static_cast<GLsizei>(tex.layers() * tex.faces());
		GLint num_levels = static_cast<GLint>(tex.levels());

		for (size_t layer = 0; layer < tex.layers(); ++layer) {
			for (size_t face = 0; face < tex.faces(); ++face) {
				for (size_t level = 0, glMip = 0; level < tex.levels(); ++level, ++glMip) {
					glm::tvec3<GLsizei> extent {tex.extent(level)};

					GLenum target = gli::is_target_cube(tex.target()) ? static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face) : glTarget;

					switch (tex.target()) {
						case gli::TARGET_1D:
							if (compressed) {
								glCompressedTexImage1D(
									target,
									static_cast<GLint>(glMip),
									glFormat.Internal,
									extent.x,
									0,
									static_cast<GLsizei>(tex.size(level)),
									tex.data(layer, face, level)
								);
							} else {
								glTexImage1D(
									target,
									static_cast<GLint>(glMip),
									glFormat.Internal,
									extent.x,
									0,
									glFormat.External,
									glFormat.Type,
									tex.data(layer, face, level)
								);
							}
							break;
						case gli::TARGET_1D_ARRAY:
						case gli::TARGET_2D:
						case gli::TARGET_CUBE:
							if (compressed) {
								glCompressedTexImage2D(
									target,
									static_cast<GLint>(glMip),
									glFormat.Internal,
									extent.x,
									tex.target() == gli::TARGET_1D_ARRAY ? static_cast<GLint>(layer) : extent.y,
									0,
									static_cast<GLsizei>(tex.size(level)),
									tex.data(layer, face, level)
								);
							} else {
								glTexImage2D(
									target,
									static_cast<GLint>(glMip),
									glFormat.Internal,
									extent.x,
									tex.target() == gli::TARGET_1D_ARRAY ? static_cast<GLint>(layer) : extent.y,
									0,
									glFormat.External,
									glFormat.Type,
									tex.data(layer, face, level)
								);
							}
							break;
						case gli::TARGET_2D_ARRAY:
						case gli::TARGET_3D:
						case gli::TARGET_CUBE_ARRAY:
							if (compressed) {
								glCompressedTexImage3D(
									target,
									static_cast<GLint>(glMip),
									glFormat.Internal,
									extent.x,
									extent.y,
									tex.target() == gli::TARGET_3D ? extent.z : static_cast<GLint>(layer),
									0,
									static_cast<GLsizei>(tex.size(level)),
									tex.data(layer, face, level)
								);
							} else {
								glTexImage3D(
									target,
									static_cast<GLint>(glMip),
									glFormat.Internal,
									extent.x,
									extent.y,
									tex.target() == gli::TARGET_3D ? extent.z : static_cast<GLint>(layer),
									0,
									glFormat.External,
									glFormat.Type,
									tex.data(layer, face, level)
								);
							}
							break;
						default:
							assert(0);
							break;
					}
				}
			}
		}

		if (glGetError() != GL_NO_ERROR) {
			LOG_ERROR("Failed to load OpenGL texture.");
			return false;
		}

		Release();
		texture = tid;
		target = glTarget;

		size = IntVector3 {
			extent.x,
			(tex.target() == gli::TARGET_1D_ARRAY) ? static_cast<int>(tex.layers()) : extent.y,
			(tex.target() == gli::TARGET_3D) ? extent.z : ((tex.target() == gli::TARGET_1D_ARRAY) ? 1 : static_cast<int>(tex.layers()))
		};
		format = texFormat;
		multisample = 1;
		numLevels = num_levels;

		// IMPROVE: Use a default global setting for samplers
		DefineSampler(FILTER_ANISOTROPIC, ADDRESS_WRAP, ADDRESS_WRAP, ADDRESS_WRAP);

		loadBuffer.reset();

		return true;
	}

	bool Texture::Define(TextureType type_, const IntVector2& size_, ImageFormat format_, int multisample_, size_t numLevels_, const ImageLevel* initialData)
	{
		return Define(type_, IntVector3 {size_.x, size_.y, 1}, format_, multisample_, numLevels_, initialData);
	}

	bool Texture::Define(TextureType type_, const IntVector3& size_, ImageFormat format_, int multisample_, size_t numLevels_, const ImageLevel* initialData)
	{
		Release();

		if (size_.x < 1 || size_.y < 1 || size_.z < 1) {
			LOG_ERROR("Texture must not have zero or negative size");
			return false;
		}
		if (type_ == TEX_2D && size_.z != 1) {
			LOG_ERROR("2D texture must have depth of 1");
			return false;
		}
		if (type_ == TEX_CUBE && (size_.x != size_.y)) {
			LOG_ERROR("Cube map must have square dimensions and 6 faces");
			return false;
		}

		if (numLevels_ < 1) {
			numLevels_ = 1;
		}
		if (multisample_ < 1) {
			multisample_ = 1;
		}

		type = type_;

		glGenTextures(1, &texture);
		if (!texture) {
			size = IntVector3::ZERO;
			format = FORMAT_NONE;
			numLevels = 0;
			multisample = 0;

			LOG_ERROR("Failed to create texture");
			return false;
		}

		glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, type != TEX_3D ? (unsigned)numLevels - 1 : 0);

		size = size_;
		format = format_;
		numLevels = numLevels_;
		multisample = multisample_;

		gli::gl GL {gli::gl::PROFILE_GL33};
		gli::gl::format glFormat = GL.translate(static_cast<gli::format>(format), gli::swizzles {0, 0, 0, 0});
		if (glFormat.Type == gli::gl::TYPE_NONE) {
			glFormat.Type = gli::gl::TYPE_U8;
		}

		target = glTargets[type];
		if (target == GL_TEXTURE_2D && multisample > 1) {
			target = GL_TEXTURE_2D_MULTISAMPLE;
		}

		ForceBind();

		// If not compressed and no initial data, create the initial level 0 texture with null data
		// Clear previous error first to be able to check whether the data was successfully set
		glGetError();
		if (!IsCompressed(format) && !initialData) {
			if (multisample == 1) {
				if (type == TEX_2D) {
					glTexImage2D(target, 0, glFormat.Internal, size.x, size.y, 0, glFormat.External, glFormat.Type, nullptr);
				} else if (type == TEX_3D) {
					glTexImage3D(target, 0, glFormat.Internal, size.x, size.y, size.z, 0, glFormat.External, glFormat.Type, nullptr);
				} else if (type == TEX_CUBE) {
					for (size_t i = 0; i < MAX_CUBE_FACES; ++i) {
						glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)i, 0, glFormat.Internal, size.x, size.y, 0, glFormat.External, glFormat.Type, nullptr);
					}
				}
			} else {
				if (type == TEX_2D) {
					glTexImage2DMultisample(target, multisample, glFormat.Internal, size.x, size.y, GL_TRUE);
				} else if (type == TEX_3D) {
					glTexImage3DMultisample(target, multisample, glFormat.Internal, size.x, size.y, size.z, GL_TRUE);
				} else if (type == TEX_CUBE) {
					for (size_t i = 0; i < MAX_CUBE_FACES; ++i) {
						glTexImage2DMultisample(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)i, multisample, glFormat.Internal, size.x, size.y, GL_TRUE);
					}
				}
			}
		}

		if (initialData) {
			for (size_t i = 0; i < numLevels; ++i) {
				if (type != TEX_3D) {
					for (int j = 0; j < size.z; ++j) {
						SetData(i, IntBox(0, 0, j, std::max(size.x >> i, 1), std::max(size.y >> i, 1), j + 1), initialData[i * size.z + j]);
					}
				} else {
					SetData(i, IntBox(0, 0, 0, std::max(size.x >> i, 1), std::max(size.y >> i, 1), std::max(size.z >> i, 1)), initialData[i]);
				}
			}
		}

		// If we have an error now, the texture was not created correctly
		if (glGetError() != GL_NO_ERROR) {
			Release();
			size = IntVector3::ZERO;
			format = FORMAT_NONE;
			numLevels = 0;

			LOG_ERROR("Failed to create texture");
			return false;
		}

		LOG_DEBUG("Created texture width {:d} height {:d} depth {:d} format {:d} numLevels {:d}", size.x, size.y, size.z, (int)format, numLevels);

		return true;
	}

	bool Texture::DefineSampler(TextureFilterMode filter_, TextureAddressMode u, TextureAddressMode v, TextureAddressMode w, unsigned maxAnisotropy_, float minLod_, float maxLod_, const Color& borderColor_)
	{
		filter = filter_;
		addressModes[0] = u;
		addressModes[1] = v;
		addressModes[2] = w;
		maxAnisotropy = maxAnisotropy_;
		minLod = minLod_;
		maxLod = maxLod_;
		borderColor = borderColor_;

		if (!texture) {
			LOG_ERROR("Texture must be defined before defining sampling parameters");
			return false;
		}

		ForceBind();

		switch (filter) {
			case FILTER_POINT:
			case COMPARE_POINT:
				glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				break;

			case FILTER_BILINEAR:
			case COMPARE_BILINEAR:
				if (numLevels < 2) {
					glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				} else {
					glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
				}
				glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				break;

			case FILTER_ANISOTROPIC:
			case FILTER_TRILINEAR:
			case COMPARE_ANISOTROPIC:
			case COMPARE_TRILINEAR:
				if (numLevels < 2) {
					glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				} else {
					glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				}
				glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				break;

			default:
				break;
		}

		glTexParameteri(target, GL_TEXTURE_WRAP_S, glWrapModes[addressModes[0]]);
		glTexParameteri(target, GL_TEXTURE_WRAP_T, glWrapModes[addressModes[1]]);
		glTexParameteri(target, GL_TEXTURE_WRAP_R, glWrapModes[addressModes[2]]);

		glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, filter == FILTER_ANISOTROPIC ? maxAnisotropy : 1.0f);

		glTexParameterf(target, GL_TEXTURE_MIN_LOD, minLod);
		glTexParameterf(target, GL_TEXTURE_MAX_LOD, maxLod);

		glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, borderColor.Data());

		if (filter >= COMPARE_POINT) {
			glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glTexParameteri(target, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		} else {
			glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		}

		return true;
	}

	bool Texture::SetData(size_t level, const IntRect& rect, const ImageLevel& data)
	{
		return SetData(level, IntBox(rect.left, rect.top, 0, rect.right, rect.bottom, 1), data);
	}

	bool Texture::SetData(size_t level, const IntBox& box, const ImageLevel& data)
	{
		if (!texture) {
			return true;
		}

		if (multisample > 1) {
			LOG_ERROR("Cannot set data on multisampled texture");
			return false;
		}

		if (level >= numLevels) {
			LOG_ERROR("Mipmap level to update out of bounds");
			return false;
		}

		IntBox levelBox(0, 0, 0, std::max(size.x >> level, 1), std::max(size.y >> level, 1), std::max(size.z >> level, 1));
		if (type == TEX_CUBE) {
			if (box.Depth() != 1) {
				LOG_ERROR("Cube map must update one face at a time");
				return false;
			}

			levelBox.near = box.near;
			levelBox.far = box.far;
		}

		if (levelBox.IsInside(box) != INSIDE) {
			assert(false);
			LOG_ERROR("Texture update region is outside level");
			return false;
		}

		ForceBind();

		bool wholeLevel = box == levelBox;

		gli::gl GL {gli::gl::PROFILE_GL33};
		gli::gl::format glFormat = GL.translate(static_cast<gli::format>(format), gli::swizzles {});
		if (glFormat.Type == gli::gl::TYPE_NONE) {
			glFormat.Type = gli::gl::TYPE_U8;
		}

		GLenum glTarget = (type == TEX_CUBE) ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + box.near : target;

		if (type != TEX_3D) {
			if (!IsCompressed(format)) {
				if (wholeLevel) {
					glTexImage2D(glTarget, (int)level, glFormat.Internal, box.Width(), box.Height(), 0, glFormat.External, glFormat.Type, data.data);
				} else {
					glTexSubImage2D(glTarget, (int)level, box.left, box.top, box.Width(), box.Height(), glFormat.External, glFormat.Type, data.data);
				}
			} else {
				if (wholeLevel) {
					glCompressedTexImage2D(glTarget, (int)level, glFormat.Internal, box.Width(), box.Height(), 0, (GLsizei)data.dataSize, data.data);
				} else {
					glCompressedTexSubImage2D(glTarget, (int)level, box.left, box.top, box.Width(), box.Height(), glFormat.External, (GLsizei)data.dataSize, data.data);
				}
			}
		} else {
			if (wholeLevel) {
				glTexImage3D(glTarget, (int)level, glFormat.Internal, box.Width(), box.Height(), box.Depth(), 0, glFormat.External, glFormat.Type, data.data);
			} else {
				glTexSubImage3D(glTarget, (int)level, box.left, box.top, box.near, box.Width(), box.Height(), box.Depth(), glFormat.External, glFormat.Type, data.data);
			}
		}

		return true;
	}

	void Texture::Bind(size_t unit)
	{
		if (unit >= MAX_TEXTURE_UNITS || !texture || BoundTextures[unit] == this) {
			return;
		}

		if (ActiveTextureUnit != unit) {
			glActiveTexture(GL_TEXTURE0 + (GLenum)unit);
			ActiveTextureUnit = unit;
		}

		if (ActiveTargets[unit] && ActiveTargets[unit] != target) {
			glBindTexture(ActiveTargets[unit], 0);
		}

		glBindTexture(target, texture);
		ActiveTargets[unit] = target;
		BoundTextures[unit] = this;
	}

	unsigned Texture::GLTarget() const
	{
		return target;
	}

	void Texture::Unbind(size_t unit)
	{
		if (BoundTextures[unit]) {
			if (ActiveTextureUnit != unit) {
				glActiveTexture(GL_TEXTURE0 + (GLenum)unit);
				ActiveTextureUnit = unit;
			}
			glBindTexture(ActiveTargets[unit], 0);
			ActiveTargets[unit] = 0;
			BoundTextures[unit] = nullptr;
		}
	}

	// ==========================================================================================
	void Texture::ForceBind()
	{
		BoundTextures[0] = nullptr;
		Bind(0);
	}

	void Texture::Release()
	{
		if (texture) {
			glDeleteTextures(1, &texture);
			texture = 0;

			for (size_t i = 0; i < MAX_TEXTURE_UNITS; ++i) {
				if (BoundTextures[i] == this) {
					BoundTextures[i] = nullptr;
				}
			}
		}
	}
}
