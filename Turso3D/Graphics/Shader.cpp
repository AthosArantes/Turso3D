#include <Turso3D/Graphics/Shader.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/IO/MemoryStream.h>
#include <Turso3D/Resource/ResourceCache.h>
#include <Turso3D/Utils/StringHash.h>
#include <GLEW/glew.h>
#include <cassert>
#include <algorithm>
#include <string_view>
#include <filesystem>
#include <type_traits>

namespace
{
	using namespace Turso3D;

	static std::string_view ReadLine(std::string_view str)
	{
		std::string_view::const_iterator eit = str.end();
		for (auto it = str.begin(); it != str.end(); ++it) {
			const char& c = *it;
			if (c == '\r' || c == '\n') {
				eit = it + 1;
				if (c == '\r' && eit != str.end() && *eit == '\n') {
					++eit;
				}
				break;
			}
		}
		return str.substr(0, static_cast<size_t>(eit - str.begin()));
	}

	static std::string_view GetShaderPragma(std::string_view line)
	{
		std::string_view directive {"pragma"};
		std::string_view value {"shader"};

		// Trim leading spaces
		while (line.size() && std::isblank(static_cast<const unsigned char&>(line.front()))) {
			line.remove_prefix(1);
		}

		if (line.front() == '#') {
			line.remove_prefix(1);
			// Trim spaces after the '#' character
			while (line.size() && std::isblank(static_cast<const unsigned char&>(line.front()))) {
				line.remove_prefix(1);
			}
			if (line.compare(0, directive.size(), directive) == 0) {
				line.remove_prefix(directive.size());
				// Trim spaces after the '#pragma'
				while (line.size() && std::isblank(static_cast<const unsigned char&>(line.front()))) {
					line.remove_prefix(1);
				}
				if (line.compare(0, value.size(), value) == 0) {
					line.remove_prefix(value.size());
					// Trim spaces after the '#pragma shader'
					while (line.size() && std::isblank(static_cast<const unsigned char&>(line.front()))) {
						line.remove_prefix(1);
					}
					return line;
				}
			}
		}

		return std::string_view {};
	}

	static std::string_view GetVersion(std::string_view line)
	{
		std::string_view directive {"version"};

		// Trim leading spaces
		while (line.size() && std::isblank(static_cast<const unsigned char&>(line.front()))) {
			line.remove_prefix(1);
		}

		if (line.front() == '#') {
			line.remove_prefix(1);
			// Trim spaces after the '#' character
			while (line.size() && std::isblank(static_cast<const unsigned char&>(line.front()))) {
				line.remove_prefix(1);
			}
			if (line.compare(0, directive.size(), directive) == 0) {
				line.remove_prefix(directive.size());
				// Trim spaces after the '#version'
				while (line.size() && std::isblank(static_cast<const unsigned char&>(line.front()))) {
					line.remove_prefix(1);
				}
				return line;
			}
		}

		return std::string_view {};
	}

	static std::string_view GetInclude(std::string_view line)
	{
		std::string_view directive {"include"};

		// Trim leading spaces
		while (line.size() && std::isblank(static_cast<const unsigned char&>(line.front()))) {
			line.remove_prefix(1);
		}

		if (line.front() == '#') {
			line.remove_prefix(1);
			// Trim spaces after the '#' character
			while (line.size() && std::isblank(static_cast<const unsigned char&>(line.front()))) {
				line.remove_prefix(1);
			}
			if (line.compare(0, directive.size(), directive) == 0) {
				line.remove_prefix(directive.size());
				// Trim spaces after the '#include'
				while (line.size() && std::isblank(static_cast<const unsigned char&>(line.front()))) {
					line.remove_prefix(1);
				}
				size_t close = std::string_view::npos;
				switch (line.front()) {
					case '<': close = line.find_first_of('>'); break;
					case '"': close = line.find_first_of('"', 1); break;
				}
				if (close != std::string_view::npos) {
					return line.substr(0, close + 1);
				}
			}
		}

		return std::string_view {};
	}

	template <typename L>
	static std::string ProcessIncludes(std::string_view str, L&& includeLambda)
	{
		static_assert(std::is_invocable_r<std::string, L, std::string_view>::value);

		std::string output {str};

		std::string_view buffer {output};
		size_t offset = 0;
		while (buffer.size()) {
			std::string_view line = ReadLine(buffer);

			std::string_view inc = GetInclude(line);
			if (inc.size()) {
				{
					// Copy all string prior the #include line.
					std::string newOutput {output.substr(0, offset)};

					// Append the include content.
					const std::string& content = includeLambda(inc);
					if (content.size()) {
						newOutput.append(content);
					}

					// Append the content post #include line.
					newOutput.append(output.substr(offset + line.size()));
					output.swap(newOutput);
				}

				// The processing should continue at the beginning of the included file.
				buffer = output;
				buffer.remove_prefix(offset);
				continue;
			}

			offset += line.size();
			buffer.remove_prefix(line.size());
		}

		return output;
	}

	// ==========================================================================================
	struct Permutation
	{
		// Defines used when compiling the shader.
		std::vector<std::string> defines;
		// The combined hash of all defines.
		size_t hash;
	};

	static void CreatePermutation(const std::string& inDefines, Permutation& outInfo)
	{
		outInfo.defines.clear();
		outInfo.hash = 0;

		std::string_view str {inDefines};
		while (str.size()) {
			// Trim leading whitespace
			while (str.size() && std::isspace(static_cast<const unsigned char&>(str.front()))) {
				str.remove_prefix(1);
			}

			// Iterate to next whitespace
			auto it = str.begin();
			for (; it != str.end(); ++it) {
				const unsigned char& c = static_cast<const unsigned char&>(*it);
				if (std::isspace(c)) {
					break;
				}
			}

			size_t count = it - str.begin();

			std::string def {str.substr(0, count)};
			if (!def.empty()) {
				outInfo.hash ^= StringHash {def};
				outInfo.defines.push_back(std::move(def));
			}

			str.remove_prefix(count);
		}
	}
}

namespace Turso3D
{
	Shader::Shader()
	{
	}

	Shader::~Shader()
	{
	}

	bool Shader::BeginLoad(Stream& source)
	{
		if (!source.IsReadable()) {
			LOG_ERROR("Failed to read shader \"{:s}\"", source.Name());
			return false;
		}

		ResourceCache* cache = ResourceCache::Instance();
		assert(cache);

		// Clear previous code
		sharedCode.clear();
		for (size_t i = 0; i < MAX_SHADER_TYPES; ++i) {
			sourceCode[i].clear();
		}

		// Process includes
		// Vector used to prevent duplicate includes.
		std::vector<StringHash> includedFiles;
		const std::string& code = ProcessIncludes(source.Read<std::string>(), [&](std::string_view include) -> std::string
		{
			std::string filename {include.substr(1, include.size() - 2)};
			if (include.front() == '"') {
				filename = std::filesystem::path {Name()}.replace_filename(filename).string();
			}
			StringHash filenameHash {filename};

			auto it = std::find(includedFiles.begin(), includedFiles.end(), filenameHash);
			if (it == includedFiles.end()) {
				std::unique_ptr<Stream> stream = cache->OpenData(filename);
				if (stream) {
					includedFiles.push_back(filenameHash);
					return stream->Read<std::string>();
				}
			}

			return std::string {};
		});

		// Process code, break by their shader stage pragmas.
		int stage = -1;
		std::string_view codev {code};
		while (codev.size()) {
			std::string_view line = ReadLine(codev);
			codev.remove_prefix(line.size());

			// Read a shader pragma to change the destination string.
			std::string_view ps = GetShaderPragma(line);
			if (ps.size()) {
				std::string_view stages[] = {{"vs"}, {"fs"}};
				for (int i = 0; i < MAX_SHADER_TYPES; ++i) {
					if (ps.compare(0, stages[i].size(), stages[i]) == 0) {
						stage = i;
						break;
					}
				}
				continue;
			}

			if (stage == -1) {
				// Extract #version directive
				std::string_view sv = GetVersion(line);
				if (sv.size()) {
					version = std::string {line};
					continue;
				}
				sharedCode.append(line);

			} else {
				sourceCode[stage].append(line);
			}
		}

		return true;
	}

	std::shared_ptr<ShaderProgram> Shader::Program(const std::string& vsDefines, const std::string& fsDefines)
	{
		Permutation permutation[MAX_SHADER_TYPES];
		CreatePermutation(vsDefines, permutation[SHADER_VS]);
		CreatePermutation(fsDefines, permutation[SHADER_FS]);

		// Check for a program already created for the shader vs/fs permutation combination.
		size_t program_hash;
		{
			size_t v = permutation[SHADER_VS].hash;
			program_hash = (v << 24 | v >> 40 & 0xFFFFFFFFFFu) ^ permutation[SHADER_FS].hash;
		}

		if (auto it = programs.find(program_hash); it != programs.end()) {
			return it->second;
		}

		// Compile shaders for program linking
		unsigned gl_shader[MAX_SHADER_TYPES];
		for (int i = 0; i < MAX_SHADER_TYPES; ++i) {
			gl_shader[i] = Compile((ShaderType)i, permutation[i].defines);

#ifdef _DEBUG
			// TODO: set a nice name
			//glObjectLabel(GL_SHADER, new_shader, name.size(), name.c_str());
#endif

		}

		std::shared_ptr<ShaderProgram> program = std::make_shared<ShaderProgram>();
		if (program->Create(gl_shader[SHADER_VS], gl_shader[SHADER_FS])) {
			programs[program_hash] = program;

#ifdef _DEBUG
			// TODO: set a nice name
			//glObjectLabel(GL_PROGRAM, program->GLProgram(), name.size(), name.c_str());
#endif

			return program;
		}

		return {};
	}

	unsigned Shader::Compile(ShaderType type, const std::vector<std::string>& defines)
	{
		std::string shader_code {version};

		GLenum gl_type;
		switch (type) {
			case SHADER_VS:
				gl_type = GL_VERTEX_SHADER;
				shader_code += "#define COMPILE_VS\n";
				break;
			case SHADER_FS:
				gl_type = GL_FRAGMENT_SHADER;
				shader_code += "#define COMPILE_FS\n";
				break;
			default:
				LOG_ERROR("Invalid shader type for compilation: {:d}", (unsigned)type);
				return 0;
		}

		for (std::string define : defines) {
			std::replace_if(define.begin(), define.end(), [](const char& c)
			{
				return c == '=';
			}, ' ');
			shader_code += "#define " + define + '\n';
		}

		shader_code.append(sharedCode);
		shader_code.append(sourceCode[type]);

		unsigned shader = glCreateShader(gl_type);
		if (!shader) {
			LOG_ERROR("Failed to create new gl shader");
			return 0;
		}

		const char* src = shader_code.c_str();
		glShaderSource(shader, 1, &src, nullptr);
		glCompileShader(shader);

		GLint compiled;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

#ifdef _DEBUG
		{
			int length, outLength;
			std::string msg;

			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
			msg.resize(length);
			glGetShaderInfoLog(shader, length, &outLength, &msg[0]);

			if (length > 1) {
				// IMPROVE: log all defines too
				LOG_DEBUG("Compiled {:s} shader {:s}: {:s}", ShaderTypeName(type), Name(), msg.c_str());
			}
		}
#endif

		if (!compiled) {
			glDeleteShader(shader);
			shader = 0;
		}

		return shader;
	}
}
