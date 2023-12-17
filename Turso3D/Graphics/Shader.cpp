#include "Shader.h"
#include <Turso3D/IO/Log.h>
#include <Turso3D/IO/MemoryStream.h>
#include <Turso3D/Resource/ResourceCache.h>
#include <cassert>
#include <algorithm>
#include <string_view>
#include <filesystem>

namespace Turso3D
{
	bool Shader::BeginLoad(Stream& source)
	{
		if (source.IsReadable()) {
			SetShaderCode(source.Read<std::string>());
			return true;
		}
		return false;
	}

	std::shared_ptr<ShaderProgram> Shader::CreateProgram(const std::string& vsDefines_, const std::string& fsDefines_)
	{
		std::vector<std::string> defines[MAX_SHADER_TYPES];
		CreateDefinesVector(vsDefines_, defines[SHADER_VS]);
		CreateDefinesVector(fsDefines_, defines[SHADER_FS]);

		std::string strDefines[MAX_SHADER_TYPES];
		StringHash hashes[MAX_SHADER_TYPES];
		for (int i = 0; i < MAX_SHADER_TYPES; ++i) {
			for (const std::string& def : defines[i]) {
				if (strDefines[i].length()) {
					strDefines[i] += ' ';
				}
				strDefines[i].append(def);
				hashes[i].value ^= StringHash {def}.value;
			}
		}

		std::pair<StringHash, StringHash> k {hashes[SHADER_VS], hashes[SHADER_FS]};
		auto it = programs.find(k);
		if (it != programs.end()) {
			return it->second;
		}

		// Create shader program
		std::shared_ptr<ShaderProgram> program = std::make_shared<ShaderProgram>();
		program->SetName(std::filesystem::path {Name()}.stem().string() + "[" + strDefines[SHADER_VS] + "][" + strDefines[SHADER_FS] + "]");

		if (program->Create(sourceCode, defines[SHADER_VS], defines[SHADER_FS])) {
			programs[k] = program;
			return program;
		}

		return {};
	}

	// ==========================================================================================
	void Shader::CreateDefinesVector(const std::string& defines, std::vector<std::string>& outVector)
	{
		if (defines.empty()) {
			return;
		}

		std::vector<std::string> buffer;

		// Split by whitespace
		size_t start = 0;
		while (start != std::string::npos) {
			// Read a define
			size_t end = defines.find_first_of(" \t", start);
			std::string_view def {&*defines.cbegin() + start, (end != std::string::npos) ? end - start : defines.length() - start};
			start = (end != std::string::npos) ? end + 1 : end;

			// Trim whitespace
			while (!def.empty() && std::isspace(def.front())) {
				def.remove_prefix(1);
			}
			while (!def.empty() && std::isspace(def.back())) {
				def.remove_suffix(1);
			}

			if (!def.empty()) {
				bool exists = false;

				// Check for duplicate defines
				for (const std::string& def2 : buffer) {
					if (def.substr(0, def.find_first_of('=')) == def2.substr(0, def2.find_first_of('='))) {
						exists = true;
						LOG_WARNING("Shader define '{:s}' was already defined for shader '{:s}'", def, Name());
						break;
					}
				}

				if (!exists) {
					buffer.push_back(std::string {def});
				}
			}
		}

		outVector.swap(buffer);
	}

	void Shader::SetShaderCode(const std::string& source)
	{
		programs.clear();
		sourceCode.clear();
		ProcessIncludes(source, sourceCode);
	}

	void Shader::ProcessIncludes(const std::string& source, std::string& outResult)
	{
		if (source.empty()) {
			return;
		}

		ResourceCache* cache = ResourceCache::Instance();
		assert(cache);

		size_t line_start = 0;
		while (line_start != std::string::npos) {
			// Read a line
			size_t line_end = source.find_first_of("\r\n", line_start);
			std::string_view line {&*source.cbegin() + line_start, (line_end != std::string::npos) ? line_end - line_start : source.length() - line_start};
			line_start = (line_end != std::string::npos) ? line_end + 1 : line_end;

			// Process include directive
			if (size_t start = line.find("#include"); start != std::string::npos) {
				start = line.find_first_of("\"<", start);
				size_t end = line.find_first_of("\">", start + 1);

				std::string_view filename = line.substr(start + 1, end - start - 1);

				std::unique_ptr<Stream> stream;
				if (line.at(start) == '<') {
					stream = cache->OpenData(std::string {filename});
				} else {
					stream = cache->OpenData(std::filesystem::path {Name()}.replace_filename(filename).string());
				}

				if (stream) {
#ifdef _DEBUG
					// Mark of included files
					outResult += "// @";
					outResult += filename;
					outResult += " {\n";
#endif

					// Add the include file into the current code recursively
					ProcessIncludes(stream->Read<std::string>(), outResult);

#ifdef _DEBUG
					outResult += "// }";
#endif
				}

			} else {
				outResult += line;
				outResult += '\n';
			}
		}
	}
}
