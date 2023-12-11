#include "ShaderProgram.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/IO/Log.h>
#include <glew/glew.h>
#include <cctype>
#include <cassert>
#include <algorithm>

namespace Turso3D
{
	static ShaderProgram* boundProgram = nullptr;

	const size_t MAX_NAME_LENGTH = 256;

	int NumberPostfix(const std::string& string)
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
		// Context may be gone at destruction time. In this case just no-op the cleanup
		if (Object::Subsystem<Graphics>()) {
			Release();
		}
	}

	bool ShaderProgram::Create(const std::string& code, const std::vector<std::string>& vsDefines, const std::vector<std::string>& fsDefines)
	{
		assert(Object::Subsystem<Graphics>()->IsInitialized());

		if (program) {
			return true;
		}

		unsigned vs = CompileShader(SHADER_VS, code, vsDefines);
		unsigned fs = CompileShader(SHADER_FS, code, fsDefines);

		if (!vs || !fs) {
			return false;
		}

		program = glCreateProgram();
		glAttachShader(program, vs);
		glAttachShader(program, fs);
		for (unsigned i = 0; i < MAX_VERTEX_ATTRIBUTES; ++i) {
			glBindAttribLocation(program, i, VertexAttributeNames[i]);
		}
		glLinkProgram(program);
		glDeleteShader(vs);
		glDeleteShader(fs);

		int linked;
		glGetProgramiv(program, GL_LINK_STATUS, &linked);
		{
			int length, outLength;
			std::string errorString;

			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
			errorString.resize(length);
			glGetProgramInfoLog(program, length, &outLength, &errorString[0]);

			if (!linked) {
				LOG_ERROR("Could not link shader \"{:s}\": {:s}", name, errorString.c_str());
				glDeleteProgram(program);
				program = 0;
				return false;
			}
#ifdef _DEBUG
			else if (length > 1) {
				LOG_DEBUG("Shader link messages \"{:s}\": {:s}", name, errorString.c_str());
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
				if (name == VertexAttributeNames[i]) {
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
			uniforms[StringHash(name)] = location;

			// Check if uniform is a preset one for quick access
			PresetUniform preset = MAX_PRESET_UNIFORMS;
			for (int i = 0; i < MAX_PRESET_UNIFORMS; ++i) {
				if (name == PresetUniformNames[i]) {
					preset = (PresetUniform)i;
					break;
				}
			}
			if (preset < MAX_PRESET_UNIFORMS) {
				presetUniforms[preset] = location;
			}

			if ((type >= GL_SAMPLER_1D && type <= GL_SAMPLER_2D_SHADOW) || (type >= GL_SAMPLER_1D_ARRAY && type <= GL_SAMPLER_CUBE_SHADOW) || (type >= GL_INT_SAMPLER_1D && type <= GL_UNSIGNED_INT_SAMPLER_2D_ARRAY)) {
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

	int ShaderProgram::Uniform(const std::string& name) const
	{
		return Uniform(StringHash(name));
	}

	int ShaderProgram::Uniform(const char* name) const
	{
		return Uniform(StringHash(name));
	}

	int ShaderProgram::Uniform(StringHash name) const
	{
		auto it = uniforms.find(name);
		return it != uniforms.end() ? it->second : -1;
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

	void ShaderProgram::SetName(const std::string& newName)
	{
		name = newName;
	}

	unsigned ShaderProgram::CompileShader(ShaderType type, const std::string& code, const std::vector<std::string>& defines)
	{
		std::string shaderCode;

		// Process shader version
		size_t offset = 0;
		if (memcmp(code.c_str(), "#version", 8) == 0) {
			offset = code.find_first_of("\n\r");
			shaderCode += code.substr(0, offset);
			shaderCode += "\n";
		} else {
			shaderCode += "#version 150\n";
		}

		GLenum glType;
		switch (type) {
			case SHADER_VS:
				glType = GL_VERTEX_SHADER;
				shaderCode += "#define COMPILE_VS\n";
				break;
			case SHADER_FS:
				glType = GL_FRAGMENT_SHADER;
				shaderCode += "#define COMPILE_FS\n";
				break;
		}

		for (std::string define : defines) {
			std::replace_if(define.begin(), define.end(), [](const char& c)
			{
				return c == '=';
			}, ' ');
			shaderCode += "#define " + define + '\n';
		}
		shaderCode += code.substr(offset);

		unsigned shader = glCreateShader(glType);
		if (!shader) {
			LOG_ERROR("Failed to create shader \"{:s}\".", name);
			return 0;
		}

		const char* src = shaderCode.c_str();
		glShaderSource(shader, 1, &src, nullptr);
		glCompileShader(shader);

		int compiled;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
		{
			int length, outLength;
			std::string msg;

			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
			msg.resize(length);
			glGetShaderInfoLog(shader, 1024, &outLength, &msg[0]);

			if (!compiled) {
				LOG_ERROR("Failed to compile shader \"{:s}\": ({:s}) {:s}", name, ShaderTypeNames[type], msg.c_str());
			}
#ifdef _DEBUG
			else if (length > 1) {
				LOG_DEBUG("Compiled shader \"{:s}\": ({:s}) {:s}", name, ShaderTypeNames[type], msg.c_str());
			}
#endif
		}

		if (!compiled) {
			glDeleteShader(shader);
			shader = 0;
		}

		return shader;
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
