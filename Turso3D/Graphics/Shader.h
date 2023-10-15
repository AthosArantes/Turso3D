#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Utils/StringHash.h>
#include <unordered_map>
#include <memory>

namespace Turso3D
{
	class Stream;

	// Shader resource.
	// Defines shader source code, from which shader programs can be compiled & linked by specifying defines.
	class Shader
	{
		friend class ResourceCache;

		struct VariationMapHasher
		{
			const size_t operator()(const std::pair<StringHash, StringHash>& value) const noexcept
			{
				return (size_t)value.first ^ ((size_t)value.first + 0x9e3779b9 + (size_t)value.second);
			}
		};
		using VariationMap = std::unordered_map<std::pair<StringHash, StringHash>, std::shared_ptr<ShaderProgram>, VariationMapHasher>;

	public:
		// Get this shader name.
		const std::string& Name() const { return name; }

		// Set shader code.
		// All existing variations are destroyed.
		bool SetShaderCode(ShaderType type, Stream& source);
		// Set shader code.
		// All existing variations are destroyed.
		bool SetShaderCode(ShaderType type, const std::string& source);

		// Return shader source code.
		const std::string& SourceCode(ShaderType type) const { return sourceCode[type]; }

		// Create and return a shader program with defines.
		// Existing program is returned if possible.
		// Variations should be cached to avoid repeated query.
		std::shared_ptr<ShaderProgram> CreateProgram(const std::string& vsDefines, const std::string& fsDefines);

	private:
		// Process include statements in the shader source code recursively.
		// Return true if successful.
		bool ProcessIncludes(std::string& code, Stream& source);

	private:
		// Shader name
		std::string name;
		// Shader source code.
		std::string sourceCode[2];
		// Shader programs.
		VariationMap programs;
	};
}
