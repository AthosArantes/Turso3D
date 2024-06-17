#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/IO/Log.h>
#include <glew/glew.h>
#include <cctype>
#include <algorithm>

namespace Turso3D
{
	static ShaderProgram* boundProgram = nullptr;

	constexpr size_t MAX_NAME_LENGTH = 256;

	static int NumberPostfix(const std::string& string)
	{
		for (size_t i = 0; i < string.length(); ++i) {
			if (isdigit(string[i])) {
				return std::strtol(string.c_str() + i, nullptr, 10);
			}
		}
		return -1;
	}

	// ==========================================================================================
	ShaderProgram::ShaderProgram() :
		program(0),
		attributes(0)
	{
		for (size_t i = 0; i < MAX_PRESET_UNIFORMS; ++i) {
			presetUniforms[i] = -1;
		}
	}

	ShaderProgram::~ShaderProgram()
	{
		Release();
	}

	bool ShaderProgram::Bind()
	{
		if (!program) {
			return false;
		}
		if (boundProgram == this) {
			return true;
		}
		glUseProgram(program);
		boundProgram = this;
		return true;
	}

	void ShaderProgram::SetUniform(PresetUniform uniform, float value)
	{
		glUniform1f(Uniform(uniform), value);
	}

	void ShaderProgram::SetUniform(PresetUniform uniform, unsigned value)
	{
		glUniform1ui(Uniform(uniform), value);
	}

	void ShaderProgram::SetUniform(PresetUniform uniform, const Vector2& value)
	{
		glUniform2fv(Uniform(uniform), 1, value.Data());
	}

	void ShaderProgram::SetUniform(PresetUniform uniform, const Vector3& value)
	{
		glUniform3fv(Uniform(uniform), 1, value.Data());
	}

	void ShaderProgram::SetUniform(PresetUniform uniform, const Vector4& value)
	{
		glUniform4fv(Uniform(uniform), 1, value.Data());
	}

	void ShaderProgram::SetUniform(PresetUniform uniform, const Matrix3& value)
	{
		glUniformMatrix3fv(Uniform(uniform), 1, GL_FALSE, value.Data());
	}

	void ShaderProgram::SetUniform(PresetUniform uniform, const Matrix3x4& value)
	{
		glUniformMatrix3x4fv(Uniform(uniform), 1, GL_FALSE, value.Data());
	}

	void ShaderProgram::SetUniform(PresetUniform uniform, const Matrix4& value)
	{
		glUniformMatrix4fv(Uniform(uniform), 1, GL_FALSE, value.Data());
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
		for (unsigned i = 0; i < MAX_VERTEX_ATTRIBUTES; ++i) {
			glBindAttribLocation(program, i, VertexAttributeName((VertexAttribute)i));
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
		int numAttributes, numUniforms, nameLength, numElements, numUniformBlocks;
		GLenum type;

		attributes = 0;

		glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &numAttributes);
		for (int i = 0; i < numAttributes; ++i) {
			glGetActiveAttrib(program, i, (GLsizei)MAX_NAME_LENGTH, &nameLength, &numElements, &type, nameBuffer);
			std::string name(nameBuffer, nameLength);

			size_t attribIndex = UINT32_MAX;
			for (int i = 0; i < MAX_VERTEX_ATTRIBUTES; ++i) {
				if (name == VertexAttributeName((VertexAttribute)i)) {
					attribIndex = i;
					break;
				}
			}
			if (attribIndex < 32) {
				attributes |= (1 << attribIndex);
			}
		}

		uniforms.clear();

		Bind();
		glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numUniforms);

		for (size_t i = 0; i < MAX_PRESET_UNIFORMS; ++i) {
			presetUniforms[i] = -1;
		}

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
			glDeleteProgram(program);
			program = 0;
			if (boundProgram == this) {
				boundProgram = nullptr;
			}
		}
	}
}
