#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/IO/Log.h>
#include <glew/glew.h>
#include <cctype>
#include <algorithm>
#include <utility>

namespace
{
	using namespace Turso3D;

	constexpr size_t MAX_NAME_LENGTH = 256;

	static std::pair<std::string_view, VertexAttributeIndex> AttributeIndices[] = {
		{"position", ATTR_POSITION},
		{"normal", ATTR_NORMAL},
		{"tangent", ATTR_TANGENT},
		{"color", ATTR_VERTEXCOLOR},
		{"texCoord", ATTR_TEXCOORD}, {"texCoord1", ATTR_TEXCOORD},
		{"texCoord2", ATTR_TEXCOORD2},
		{"blendWeights", ATTR_BLENDWEIGHTS},
		{"blendIndices", ATTR_BLENDINDICES},
		{"worldInstanceM0", ATTR_WORLDINSTANCE_M0}, {"texCoord3", ATTR_WORLDINSTANCE_M0},
		{"worldInstanceM1", ATTR_WORLDINSTANCE_M1}, {"texCoord4", ATTR_WORLDINSTANCE_M1},
		{"worldInstanceM2", ATTR_WORLDINSTANCE_M2}, {"texCoord5", ATTR_WORLDINSTANCE_M2},
		{"instanceData0", ATTR_INSTANCE_DATA0},
		{"instanceData1", ATTR_INSTANCE_DATA1}
	};

	static const char* PresetUniformName(PresetUniform value)
	{
		constexpr const char* data[] = {
			"worldMatrix",
			"lightMask",
			nullptr
		};
		return data[value];
	}

	static int NumberPostfix(const std::string& string)
	{
		for (size_t i = 0; i < string.length(); ++i) {
			if (isdigit(string[i])) {
				return std::strtol(string.c_str() + i, nullptr, 10);
			}
		}
		return -1;
	}
}

namespace Turso3D
{
	ShaderProgram::ShaderProgram() :
		program(0)
	{
		for (size_t i = 0; i < MAX_PRESET_UNIFORMS; ++i) {
			presetUniforms[i] = -1;
		}
	}

	ShaderProgram::~ShaderProgram()
	{
		Release();
	}

	void ShaderProgram::SetUniform(PresetUniform uniform, float value)
	{
		glUniform1f(presetUniforms[uniform], value);
	}

	void ShaderProgram::SetUniform(PresetUniform uniform, unsigned value)
	{
		glUniform1ui(presetUniforms[uniform], value);
	}

	void ShaderProgram::SetUniform(PresetUniform uniform, const Vector2& value)
	{
		glUniform2fv(presetUniforms[uniform], 1, value.Data());
	}

	void ShaderProgram::SetUniform(PresetUniform uniform, const Vector3& value)
	{
		glUniform3fv(presetUniforms[uniform], 1, value.Data());
	}

	void ShaderProgram::SetUniform(PresetUniform uniform, const Vector4& value)
	{
		glUniform4fv(presetUniforms[uniform], 1, value.Data());
	}

	void ShaderProgram::SetUniform(PresetUniform uniform, const Matrix3& value)
	{
		glUniformMatrix3fv(presetUniforms[uniform], 1, GL_FALSE, value.Data());
	}

	void ShaderProgram::SetUniform(PresetUniform uniform, const Matrix3x4& value)
	{
		glUniformMatrix3x4fv(presetUniforms[uniform], 1, GL_FALSE, value.Data());
	}

	void ShaderProgram::SetUniform(PresetUniform uniform, const Matrix4& value)
	{
		glUniformMatrix4fv(presetUniforms[uniform], 1, GL_FALSE, value.Data());
	}

	bool ShaderProgram::Create(unsigned vs, unsigned fs)
	{
		if (program) {
			return true;
		}

		if (!vs || !fs) {
			return false;
		}

		program = glCreateProgram();
		glAttachShader(program, vs);
		glAttachShader(program, fs);

		// Explicitly define vertex attribute indices
		for (int i = 0; i < std::size(AttributeIndices); ++i) {
			glBindAttribLocation(program, static_cast<GLuint>(AttributeIndices[i].second), AttributeIndices[i].first.data());
		}
		glLinkProgram(program);

		glDeleteShader(vs);
		glDeleteShader(fs);

		GLint linked;
		glGetProgramiv(program, GL_LINK_STATUS, &linked);
		{
			int length, outLength;
			std::string errorString;

			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
			errorString.resize(length);
			glGetProgramInfoLog(program, length, &outLength, &errorString[0]);

			if (!linked) {
				LOG_ERROR("Could not link shader program: {:s}", errorString.c_str());
				glDeleteProgram(program);
				program = 0;
				return false;
			}
#ifdef _DEBUG
			else if (length > 1) {
				LOG_DEBUG("Shader program link messages: {:s}", errorString.c_str());
			}
#endif
		}

		char nameBuffer[MAX_NAME_LENGTH];
		int numUniforms, nameLength, numElements, numUniformBlocks;
		GLenum type;

		Graphics::BindProgram(this);
		glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numUniforms);

		for (size_t i = 0; i < MAX_PRESET_UNIFORMS; ++i) {
			presetUniforms[i] = -1;
		}
		uniforms.clear();

		for (int i = 0; i < numUniforms; ++i) {
			glGetActiveUniform(program, i, MAX_NAME_LENGTH, &nameLength, &numElements, &type, nameBuffer);
			std::string name(nameBuffer, nameLength);

			int location = glGetUniformLocation(program, name.c_str());
			if (size_t p = name.find("[0]", 0); p != std::string::npos) {
				name.erase(p, 3);
			}
			uniforms[StringHash {name}] = location;

			// Check if uniform is a preset one for quick access
			PresetUniform preset = MAX_PRESET_UNIFORMS;
			for (int i = 0; i < MAX_PRESET_UNIFORMS; ++i) {
				if (name == PresetUniformName((PresetUniform)i)) {
					preset = (PresetUniform)i;
					break;
				}
			}
			if (preset < MAX_PRESET_UNIFORMS) {
				presetUniforms[preset] = location;
			}

			if (
				(type >= GL_SAMPLER_1D && type <= GL_SAMPLER_2D_SHADOW) ||
				(type >= GL_SAMPLER_1D_ARRAY && type <= GL_SAMPLER_CUBE_SHADOW) ||
				(type >= GL_INT_SAMPLER_1D && type <= GL_UNSIGNED_INT_SAMPLER_2D_ARRAY) ||
				(type >= GL_SAMPLER_CUBE_MAP_ARRAY && type <= GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY)
			) {
				// Assign sampler uniforms to a texture unit according to the number appended to the sampler name
				int unit = NumberPostfix(name);
				if (unit < 0) {
					continue;
				}

				// Array samplers may have multiple elements, assign each sequentially
				if (numElements > 1) {
					std::vector<int> units;
					for (int j = 0; j < numElements; ++j) {
						units.push_back(unit++);
					}
					glUniform1iv(location, numElements, &units[0]);
				} else {
					glUniform1iv(location, 1, &unit);
				}
			}
		}

		glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &numUniformBlocks);
		for (int i = 0; i < numUniformBlocks; ++i) {
			glGetActiveUniformBlockName(program, i, MAX_NAME_LENGTH, &nameLength, nameBuffer);
			std::string name(nameBuffer, nameLength);

			int blockIndex = glGetUniformBlockIndex(program, name.c_str());
			int bindingIndex = NumberPostfix(name);
			// If no number postfix in the name, use the block index
			if (bindingIndex < 0) {
				bindingIndex = blockIndex;
			}

			glUniformBlockBinding(program, blockIndex, bindingIndex);
		}

		return true;
	}

	void ShaderProgram::Release()
	{
		if (program) {
			Graphics::BindProgram(nullptr);
			glDeleteProgram(program);
			program = 0;
		}
	}
}
