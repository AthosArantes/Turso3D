#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/IO/Stream.h>
#include <Turso3D/IO/Log.h>
#include <glew/glew.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <gli/load.hpp>
#include <gli/generate_mipmaps.hpp>

namespace Turso3D
{
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

	struct Texture::LoadBuffer
	{
		gli::texture texture;
		IntVector3 size;
		std::vector<ImageLevel> imageData;
	};

	// ==========================================================================================
	static size_t ActiveTextureUnit = 0xffffffff;
	static unsigned ActiveTargets[MAX_TEXTURE_UNITS];
	static Texture* BoundTextures[MAX_TEXTURE_UNITS];

	static gli::gl GLIProfile {gli::gl::PROFILE_GL33};

	// ==========================================================================================
	Texture::Texture() :
		texture(0),
		target(0),
		type(TARGET_2D),
		size(IntVector3::ZERO()),
		format(FORMAT_NONE),
		multisample(0),
		numLevels(0),
		loadFlags(0)
	{
	}

	Texture::Texture(unsigned loadFlags) :
		texture(0),
		target(0),
		type(TARGET_2D),
		size(IntVector3::ZERO()),
		format(FORMAT_NONE),
		multisample(0),
		numLevels(0),
		loadFlags(loadFlags)
	{
	}

	Texture::~Texture()
	{
		Release();
	}

	bool Texture::BeginLoad(Stream& source)
	{
		loadBuffer = std::make_unique<LoadBuffer>();
		{
			size_t sz = source.Size();
			std::unique_ptr<char[]> data = std::make_unique<char[]>(sz);
			size_t length = source.Read(data.get(), sz);

			loadBuffer->texture = gli::load(data.get(), length);
		}

		// The gli texture buffer
		gli::texture& texture = loadBuffer->texture;
		std::vector<ImageLevel>& imageLevels = loadBuffer->imageData;

		if (texture.empty()) {
			LOG_ERROR("Failed to load texture from \"{:s}\"", source.Name());
			loadBuffer.reset();
			return false;
		}

		bool compressed = gli::is_compressed(texture.format());

		// Generate mip maps
		if (!compressed && texture.levels() == 1 && (loadFlags & LOAD_FLAG_GENERATE_MIPS)) {
			gli::filter filter = gli::FILTER_LINEAR;
			switch (texture.target()) {
				case gli::TARGET_1D:
					texture = gli::generate_mipmaps(gli::texture1d {texture}, filter);
					break;
				case gli::TARGET_1D_ARRAY:
					texture = gli::generate_mipmaps(gli::texture1d_array {texture}, filter);
					break;
				case gli::TARGET_2D:
					texture = gli::generate_mipmaps(gli::texture2d {texture}, filter);
					break;
				case gli::TARGET_2D_ARRAY:
					texture = gli::generate_mipmaps(gli::texture2d_array {texture}, filter);
					break;
				case gli::TARGET_3D:
					texture = gli::generate_mipmaps(gli::texture3d {texture}, filter);
					break;
				case gli::TARGET_CUBE:
					texture = gli::generate_mipmaps(gli::texture_cube {texture}, filter);
					break;
				case gli::TARGET_CUBE_ARRAY:
					texture = gli::generate_mipmaps(gli::texture_cube_array {texture}, filter);
					break;
			}
		}

		// Retrieve the texture size
		{
			glm::ivec3 extent = texture.extent();
			loadBuffer->size = IntVector3 {extent.x, extent.y, extent.z};
		}
		switch (texture.target()) {
			case gli::TARGET_1D_ARRAY:
				loadBuffer->size.y = static_cast<int>(texture.layers());
				break;
			case gli::TARGET_2D_ARRAY:
				loadBuffer->size.z = static_cast<int>(texture.layers());
				break;
			case gli::TARGET_CUBE:
			case gli::TARGET_CUBE_ARRAY:
				loadBuffer->size.z = static_cast<int>(texture.layers() * texture.faces());
				break;
		}

		size_t face_count = texture.layers() * texture.faces();

		for (size_t layer = 0; layer < texture.layers(); ++layer) {
			for (size_t face = 0; face < texture.faces(); ++face) {
				for (size_t level = 0; level < texture.levels(); ++level) {
					ImageLevel& image = imageLevels.emplace_back();
					image.data = texture.data(layer, face, level);
					image.dataSize = compressed ? static_cast<int>(texture.size(level)) : 0;
					image.layer_face = static_cast<int>(face_count * layer + face);
					image.level = static_cast<int>(level);

					glm::ivec3 extent {texture.extent(level)};
					switch (texture.target()) {
						case gli::TARGET_1D:
						case gli::TARGET_1D_ARRAY:
							image.dimensions = IntBox {0, 0, 0, extent.x, 0, 0};
							break;
						case gli::TARGET_2D:
						case gli::TARGET_2D_ARRAY:
						case gli::TARGET_CUBE:
						case gli::TARGET_CUBE_ARRAY:
							image.dimensions = IntBox {0, 0, 0, extent.x, extent.y, 0};
							break;
						case gli::TARGET_3D:
							image.dimensions = IntBox {0, 0, 0, extent.x, extent.y, extent.z};
							break;
						default:
							LOG_ERROR("Unsupported texture format");
							loadBuffer.reset();
							return false;
					}
				}
			}
		}

		return true;
	}

	bool Texture::EndLoad()
	{
		if (!loadBuffer) {
			return false;
		}

		// The gli texture buffer
		gli::texture& texture = loadBuffer->texture;

		TextureTarget type = static_cast<TextureTarget>(texture.target());
		ImageFormat format = static_cast<ImageFormat>(texture.format());
		if (loadFlags & LOAD_FLAG_SRGB) {
			// sRGB override
			format = GetSRGBFormat(format);
		}

		bool success = false;
		if (Define(type, loadBuffer->size, format, 1, texture.levels())) {
			gli::gl::format glFormat = GLIProfile.translate(texture.format(), texture.swizzles());
			glTexParameteri(target, GL_TEXTURE_SWIZZLE_R, glFormat.Swizzles[0]);
			glTexParameteri(target, GL_TEXTURE_SWIZZLE_G, glFormat.Swizzles[1]);
			glTexParameteri(target, GL_TEXTURE_SWIZZLE_B, glFormat.Swizzles[2]);
			glTexParameteri(target, GL_TEXTURE_SWIZZLE_A, glFormat.Swizzles[3]);

			for (const ImageLevel& il : loadBuffer->imageData) {
				SetData(il);
			}

			// IMPROVE: Use a default global setting for samplers
			success = DefineSampler(FILTER_ANISOTROPIC, ADDRESS_WRAP, ADDRESS_WRAP, ADDRESS_WRAP);
		}

		loadBuffer.reset();
		return success;
	}

	bool Texture::Define(TextureTarget type, const IntVector3& size, ImageFormat format, int multisample, int numLevels)
	{
		Release();

		if ((type == TARGET_CUBE || type == TARGET_CUBE_ARRAY) && (size.x != size.y || size.z % MAX_CUBE_FACES != 0)) {
			LOG_ERROR("Cube map must have square dimensions and have all 6 faces.");
			return false;
		}

#ifdef _DEBUG
		// Clear GL errors
		unsigned err;
		do {
			err = glGetError();
		} while (err != GL_NO_ERROR);
#endif

		if (multisample < 1) {
			multisample = 1;
		}
		if (numLevels < 1 || multisample > 1) {
			numLevels = 1;
		}

		// Validate multisampled target types
		target = GLIProfile.translate(static_cast<gli::target>(type));
		if (multisample > 1) {
			switch (type) {
				case TARGET_2D:
					target = GL_TEXTURE_2D_MULTISAMPLE;
					break;
				case TARGET_2D_ARRAY:
					target = GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
					break;
				case TARGET_3D:
				case TARGET_CUBE:
				case TARGET_CUBE_ARRAY:
					break;
				default:
#ifdef _DEBUG
					LOG_ERROR("Multisample not supported for texture type ({:s})", TextureTargetName(type));
#else
					LOG_ERROR("Multisample not supported for texture type ({:d})", (unsigned)type);
#endif
					return false;
			}
		}

		glGenTextures(1, &texture);
		if (!texture) {
			this->size = IntVector3::ZERO();
			this->format = FORMAT_NONE;
			this->multisample = 0;
			this->numLevels = 0;

			LOG_ERROR("Failed to create opengl texture");
			return false;
		}
		ForceBind();

		if (multisample == 1) {
			glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, numLevels - 1);
		}

		this->type = type;
		this->size = size;
		this->format = format;
		this->multisample = multisample;
		this->numLevels = numLevels;

		gli::gl::format glFormat = GLIProfile.translate(static_cast<gli::format>(format), gli::swizzles {});

		if (GLEW_VERSION_4_2 || (GLEW_ARB_texture_storage && GLEW_ARB_texture_storage_multisample)) {
			switch (type) {
				case TARGET_1D:
					glTexStorage1D(target, numLevels, glFormat.Internal, size.x);
					break;
				case TARGET_1D_ARRAY:
				case TARGET_2D:
				case TARGET_CUBE:
					if (multisample > 1) {
						glTexStorage2DMultisample(target, multisample, glFormat.Internal, size.x, size.y, GL_TRUE);
					} else {
						glTexStorage2D(target, numLevels, glFormat.Internal, size.x, size.y);
					}
					break;
				case TARGET_2D_ARRAY:
				case TARGET_3D:
				case TARGET_CUBE_ARRAY:
					if (multisample > 1) {
						glTexStorage3DMultisample(target, multisample, glFormat.Internal, size.x, size.y, size.z, GL_TRUE);
					} else {
						glTexStorage3D(target, numLevels, glFormat.Internal, size.x, size.y, size.z);
					}
					break;
			}

		} else {
			bool compressed = IsCompressed(format);

			// Set valid values
			glFormat.External = (glFormat.External == gli::gl::EXTERNAL_NONE) ? gli::gl::EXTERNAL_RGBA : glFormat.External;
			glFormat.Type = (glFormat.Type == gli::gl::TYPE_NONE) ? gli::gl::TYPE_U8 : glFormat.Type;

			for (int level = 0; level < (int)numLevels; ++level) {
				int w = std::max(size.x >> level, 1);
				int h = std::max(size.y >> level, 1);
				int d = std::max(size.z >> level, 1);

				switch (type) {
					case TARGET_1D:
						glTexImage1D(target, level, glFormat.Internal, w, 0, glFormat.External, glFormat.Type, nullptr);
						break;
					case TARGET_1D_ARRAY:
					case TARGET_2D:
					case TARGET_CUBE:
						for (unsigned i = 0; i < ((type == TARGET_CUBE) ? MAX_CUBE_FACES : 1); ++i) {
							GLenum t = (type == TARGET_CUBE) ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)i : target;
							if (multisample == 1) {
								glTexImage2D(t, level, glFormat.Internal, w, h, 0, glFormat.External, glFormat.Type, nullptr);
							} else {
								glTexImage2DMultisample(t, multisample, glFormat.Internal, w, h, GL_TRUE);
							}
						}
						break;
					case TARGET_2D_ARRAY:
					case TARGET_3D:
					case TARGET_CUBE_ARRAY:
					{
						d = (type == TARGET_CUBE_ARRAY) ? size.z : d;
						if (multisample == 1) {
							glTexImage3D(target, level, glFormat.Internal, w, h, d, 0, glFormat.External, glFormat.Type, nullptr);
						} else {
							glTexImage3DMultisample(target, multisample, glFormat.Internal, w, h, d, GL_TRUE);
						}
						break;
					}
					default:
#ifdef _DEBUG
						LOG_ERROR("Texture type ({:s}) is not supported.", TextureTargetName(type));
#else
						LOG_ERROR("Texture type ({:d}) is not supported.", (unsigned)type);
#endif
						Release();
						return false;
				}
			}
		}

#ifdef _DEBUG
		const char* target_name = TextureTargetName(type);
		const char* format_name = ImageFormatName(format);

		if (glGetError() != GL_NO_ERROR) {
			LOG_ERROR("Failed to create {:s} ({:s})", target_name, Name());
			return false;
		}

		std::string name;
		if (!Name().empty()) {
			name = fmt::format(" ({:s})", Name());
		}

		switch (type) {
			case TARGET_1D:
				LOG_DEBUG("Created {:s}{:s} {:s} [{:d}] [Mips:{:d}]", target_name, name, format_name, size.x, numLevels);
				break;
			case TARGET_1D_ARRAY:
			case TARGET_2D:
			case TARGET_CUBE:
				LOG_DEBUG("Created {:s}{:s} {:s} [{:d} x {:d}] [Mips:{:d}]", target_name, name, format_name, size.x, size.y, numLevels);
				break;
			case TARGET_2D_ARRAY:
			case TARGET_3D:
			case TARGET_CUBE_ARRAY:
				LOG_DEBUG("Created {:s}{:s} {:s} [{:d} x {:d} x {:d}] [Mips:{:d}]", target_name, name, format_name, size.x, size.y, size.z, numLevels);
				break;
		}
#else
		LOG_INFO("Created texture ({:s}) [Type:{:d}] [Format:{:d}] [{:d} x {:d} x {:d}] [Mips:{:d}]", Name(), (int)type, (int)format, size.x, size.y, size.z, numLevels);
#endif

		return true;
	}

	bool Texture::DefineSampler(TextureFilterMode filter, TextureAddressMode u, TextureAddressMode v, TextureAddressMode w, unsigned maxAnisotropy, float minLod, float maxLod, const Color& borderColor)
	{
		if (!texture) {
			LOG_ERROR("Texture must be defined before defining sampling parameters");
			return false;
		}

		if (multisample > 1) {
			LOG_ERROR("Multisample textures doesn't support sampler state.");
			return false;
		}

		this->filter = filter;
		addressModes[0] = u;
		addressModes[1] = v;
		addressModes[2] = w;
		this->maxAnisotropy = maxAnisotropy;
		this->minLod = minLod;
		this->maxLod = maxLod;
		this->borderColor = borderColor;

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

	bool Texture::SetData(const ImageLevel& data)
	{
		if (!texture) {
			return true;
		}

		if (multisample > 1) {
			LOG_ERROR("Cannot set data on multisampled texture");
			return false;
		}

		if (data.level >= numLevels) {
			LOG_ERROR("Mipmap level to update out of bounds");
			return false;
		}

		// Check image boundaries
		IntBox dstBox;
		IntBox box = data.dimensions;
		switch (type) {
			case TARGET_1D:
			case TARGET_1D_ARRAY:
				dstBox = IntBox {0, 0, 0, std::max(size.x >> data.level, 1), 1, 1};
				box.left = box.near = 0;
				box.right = box.far = 1;
				break;
			case TARGET_2D:
			case TARGET_2D_ARRAY:
			case TARGET_CUBE:
			case TARGET_CUBE_ARRAY:
				dstBox = IntBox {0, 0, 0, std::max(size.x >> data.level, 1), std::max(size.y >> data.level, 1), 1};
				box.near = 0;
				box.far = 1;
				break;
			case TARGET_3D:
				dstBox = IntBox {0, 0, 0, std::max(size.x >> data.level, 1), std::max(size.y >> data.level, 1), std::max(size.z >> data.level, 1)};
				break;
		}

		if (dstBox.IsInside(box) != INSIDE) {
			assert(false);
			LOG_ERROR("Texture update region is outside level");
			return false;
		}

		ForceBind();

		gli::gl::format glFormat = GLIProfile.translate(static_cast<gli::format>(format), gli::swizzles {0, 0, 0, 0});
		bool compressed = IsCompressed(format);

		switch (type) {
			case TARGET_1D:
				if (!compressed) {
					glTexSubImage1D(target, data.level, box.left, box.Width(), glFormat.External, glFormat.Type, data.data);
				} else {
					glCompressedTexSubImage1D(target, data.level, box.left, box.Width(), glFormat.Internal, data.dataSize, data.data);
				}
				break;
			case TARGET_1D_ARRAY:
			case TARGET_2D:
			case TARGET_CUBE:
			{
				unsigned t = (type == TARGET_CUBE) ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + data.layer_face : target;
				int y = (type == TARGET_1D_ARRAY) ? data.layer_face : box.top;
				int h = (type == TARGET_1D_ARRAY) ? 1 : box.Height();
				if (!compressed) {
					glTexSubImage2D(t, data.level, box.left, y, box.Width(), h, glFormat.External, glFormat.Type, data.data);
				} else {
					glCompressedTexSubImage2D(t, data.level, box.left, y, box.Width(), h, glFormat.Internal, data.dataSize, data.data);
				}
				break;
			}
			case TARGET_2D_ARRAY:
			case TARGET_3D:
			case TARGET_CUBE_ARRAY:
			{
				int z = (type == TARGET_3D) ? box.near : data.layer_face;
				int d = (type == TARGET_3D) ? box.Depth() : 1;
				if (!compressed) {
					glTexSubImage3D(target, data.level, box.left, box.top, z, box.Width(), box.Height(), d, glFormat.External, glFormat.Type, data.data);
				} else {
					glCompressedTexSubImage3D(target, data.level, box.left, box.top, z, box.Width(), box.Height(), d, glFormat.Internal, data.dataSize, data.data);
				}
				break;
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

	// ==========================================================================================
	void Texture::Unbind(size_t unit)
	{
		if (BoundTextures[unit]) {
			if (ActiveTextureUnit != unit) {
				glActiveTexture(GL_TEXTURE0 + (GLenum)unit);
				ActiveTextureUnit = unit;
			}
			if (ActiveTargets[unit]) {
				glBindTexture(ActiveTargets[unit], 0);
				ActiveTargets[unit] = 0;
			}
			BoundTextures[unit] = nullptr;
		}
	}

	unsigned Texture::GetGLInternalFormat(ImageFormat format)
	{
		gli::gl::format glFormat = GLIProfile.translate(static_cast<gli::format>(format), gli::swizzles {});
		return glFormat.Internal;
	}

	bool Texture::IsCompressed(ImageFormat format)
	{
		gli::format f = static_cast<gli::format>(format);
		return gli::is_compressed(f);
	}

	bool Texture::IsStencil(ImageFormat format)
	{
		gli::format f = static_cast<gli::format>(format);
		return gli::is_stencil(f);
	}

	size_t Texture::BitsPerPixel(ImageFormat format)
	{
		gli::format f = static_cast<gli::format>(format);
		return gli::bits_per_pixel(f);
	}
}
