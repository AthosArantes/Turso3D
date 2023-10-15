#include "Shader.h"
#include <Turso3D/IO/Log.h>
#include <Turso3D/IO/MemoryStream.h>
#include <Turso3D/Resource/ResourceCache.h>
#include <Turso3D/Utils/StringUtils.h>
#include <cassert>
#include <algorithm>
#include <filesystem>

namespace Turso3D
{
	namespace fs = std::filesystem;

	// Sort the defines and remove defines that are not present in shader code.
	static void NormalizeDefines(const std::string& code, std::vector<std::string>& defines)
	{
		std::sort(defines.begin(), defines.end());

		// Remove defines that are not used in the shader code.
		for (auto it = defines.begin(); it != defines.end();) {
			bool used = false;

			std::string& str = *it;
			size_t equalsPos = str.find('=');
			if (equalsPos == std::string::npos) {
				if (code.find(str) != std::string::npos) {
					used = true;
				}
			} else {
				std::string beginPart = str.substr(0, equalsPos);
				if (code.find(beginPart) != std::string::npos) {
					used = true;
				}
			}

			if (!used) {
				it = defines.erase(it);
			} else {
				++it;
			}
		}
	}

	// ==========================================================================================
	bool Shader::SetShaderCode(ShaderType type, Stream& source)
	{
		if (!source.IsReadable()) {
			return false;
		}

		programs.clear();

		std::string& code = sourceCode[type];
		code.clear();

		return ProcessIncludes(code, source);
	}

	bool Shader::SetShaderCode(ShaderType type, const std::string& source)
	{
		MemoryStream stream;
		stream.Resize(source.size());
		stream.Write(source.c_str(), source.size());
		stream.Seek(0);
		return SetShaderCode(type, stream);
	}

	std::shared_ptr<ShaderProgram> Shader::CreateProgram(const std::string& vsDefines_, const std::string& fsDefines_)
	{
		auto hash {std::make_pair(StringHash(vsDefines_), StringHash(fsDefines_))};

		auto it = programs.find(hash);
		if (it != programs.end()) {
			return it->second;
		}

		// If initially not found, normalize the defines and try again.

		std::vector<std::string> vsDefines {SplitStringWhitespace(vsDefines_)};
		NormalizeDefines(sourceCode[SHADER_VS], vsDefines);
		std::string vsDefinesStr {JoinString(vsDefines, ' ')};

		std::vector<std::string> fsDefines {SplitStringWhitespace(fsDefines_)};
		NormalizeDefines(sourceCode[SHADER_FS], fsDefines);
		std::string fsDefinesStr {JoinString(fsDefines, ' ')};

		auto normalizedHash {std::make_pair(StringHash(vsDefinesStr), StringHash(fsDefinesStr))};
		it = programs.find(normalizedHash);
		if (it != programs.end()) {
			return it->second;
		}

		// Create shader program
		std::shared_ptr<ShaderProgram> newVariation = std::make_shared<ShaderProgram>();
		newVariation->SetName(fs::path(name).stem().string() + "[" + vsDefinesStr + "][" + fsDefinesStr + "]");

		if (newVariation->Create(sourceCode[SHADER_VS], sourceCode[SHADER_FS], vsDefines, fsDefines)) {
			programs[hash] = newVariation;
			programs[normalizedHash] = newVariation;
			return newVariation;
		}

		return nullptr;
	}

	bool Shader::ProcessIncludes(std::string& code, Stream& source)
	{
		const std::string directive {"#include"};

		ResourceCache* cache = Object::Subsystem<ResourceCache>();
		assert(cache);

		while (!source.IsEof()) {
			std::string line {source.ReadLine()};

			if (size_t start = line.find(directive); start != std::string::npos) {
				start = line.find_first_of("\"<", start);
				size_t end = line.find_first_of("\">", start + 1);

				std::string filename = line.substr(start + 1, end - start - 1);

				std::unique_ptr<Stream> stream;
				if (line.at(start) == '<') {
					stream = cache->OpenResource(filename);
				} else {
					stream = cache->OpenResource((fs::path(Name()).remove_filename() / filename).string());
				}

				if (!stream) {
					return false;
				}

				// Mark of included files
#ifdef _DEBUG
				code += "// ===========================\n";
				code += "// Including: " + filename + "\n";
				code += "// ===========================\n";
#endif
				code += '\n';

				// Add the include file into the current code recursively
				if (!ProcessIncludes(code, *stream)) {
					return false;
				}
			} else {
				code += line;
				code += '\n';
			}
		}

		return true;
	}
}
