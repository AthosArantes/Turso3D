#include <Turso3D/Graphics/Shader.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/IO/MemoryStream.h>
#include <Turso3D/Resource/ResourceCache.h>
#include <Turso3D/Utils/StringHash.h>
#include <Turso3D/Utils/ShaderPermutation.h>
#include <GLEW/glew.h>
#include <cassert>
#include <algorithm>
#include <string_view>
#include <filesystem>
#include <type_traits>

namespace
{
	using namespace Turso3D;

	static const char* ShaderTypeName(ShaderType value)
	{
		constexpr const char* data[] = {
			"VS",
			"FS",
			nullptr
		};
		return data[value];
	}

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

	static ShaderType GetShaderPragma(std::string_view line)
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

					if (line.front() == ':') {
						line.remove_prefix(1);

						size_t sz = 0;
						while ((line.size() - sz) && !std::isblank(static_cast<const unsigned char&>(line.at(sz)))) {
							++sz;
						}

						std::string_view stage = line.substr(0, sz);
						if (stage.size()) {
							for (size_t i = 0; i < MAX_SHADER_TYPES; ++i) {
								ShaderType t = static_cast<ShaderType>(i);
								if (stage == ShaderTypeName(t)) {
									return t;
								}
							}
						}

#ifdef _DEBUG
						LOG_ERROR("Invalid pragma shader type specified: \"{:s}\"", stage);
#endif
					}
				}
			}
		}

		return MAX_SHADER_TYPES;
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
}

namespace Turso3D
{
	struct Shader::LinkedProgram
	{
		// The linked shader program.
		std::shared_ptr<ShaderProgram> program;
		// The shader defines.
		//ShaderPermutation permutation;
	};
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
			ShaderType toStage = GetShaderPragma(line);
			if (toStage != MAX_SHADER_TYPES) {
				stage = toStage;
				continue;
			}

			if (stage == -1) {
				// Extract #version directive (only from the common code section).
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

	std::shared_ptr<ShaderProgram> Shader::Program(const ShaderPermutation& vsPermutation, const ShaderPermutation& fsPermutation)
	{
		// Check for a program already created for the vs/fs permutation combination.
		size_t program_hash;
		{
			size_t v = vsPermutation.Hash();
			program_hash = (v << 24 | v >> 40 & 0xFFFFFFFFFFu) ^ fsPermutation.Hash();
		}

		if (auto it = programs.find(program_hash); it != programs.end()) {
			return it->second.program;
		}

		// Compile shaders for program linking
		unsigned vs = Compile(SHADER_VS, vsPermutation);
		unsigned fs = Compile(SHADER_FS, fsPermutation);

		std::shared_ptr<ShaderProgram> program = std::make_shared<ShaderProgram>();
		if (program->Create(vs, fs)) {
			programs[program_hash].program = program;

#ifdef _DEBUG
			std::string defines[MAX_SHADER_TYPES];
			const ShaderPermutation* permutation[] = {&vsPermutation, &fsPermutation};
			for (size_t i = 0; i < MAX_SHADER_TYPES; ++i) {
				for (const std::string_view& def : permutation[i]->Defines()) {
					if (defines[i].length()) {
						defines[i].push_back(';');
					}
					defines[i].append(def);
				}
			}
			std::string name = fmt::format("{:s}[{:s}][{:s}]", Name(), defines[SHADER_VS], defines[SHADER_FS]);
			glObjectLabel(GL_PROGRAM, program->GLProgram(), name.size(), name.c_str());
#endif

			return program;
		}

		return std::shared_ptr<ShaderProgram> {};
	}

	unsigned Shader::Compile(ShaderType type, const ShaderPermutation& permutation)
	{
		std::string shader_code;
		shader_code.reserve(version.length() + sharedCode.length() + sourceCode[type].length());

		shader_code.append(version);

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

		const std::vector<std::string_view>& defines = permutation.Defines();
		for (size_t i = 0; i < defines.size(); ++i) {
			std::string_view define = defines[i];

			shader_code.append("#define ");

			std::string_view v = ShaderPermutation::ValuePart(define);
			if (v.empty()) {
				shader_code.append(define);
			} else {
				std::string_view n = ShaderPermutation::NamePart(define);
				shader_code.append(n);
				shader_code.push_back(' ');
				shader_code.append(v);
			}

			shader_code.push_back('\n');
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
