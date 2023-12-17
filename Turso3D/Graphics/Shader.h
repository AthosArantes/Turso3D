#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Resource/Resource.h>
#include <Turso3D/Utils/StringHash.h>
#include <unordered_map>
#include <memory>
#include <vector>

namespace Turso3D
{
	class Stream;

	// Shader resource.
	// Defines shader source code, from which shader programs can be compiled & linked by specifying defines.
	class Shader : public Resource
	{
		struct VariationMapHasher
		{
			const size_t operator()(const std::pair<StringHash, StringHash>& value) const noexcept
			{
				return (size_t)value.first ^ ((size_t)value.first + 0x9e3779b9 + (size_t)value.second);
			}
		};
		// Maps a pair of vsDefines hash and fsDefines hash to a ShaderProgram.
		using VariationMap = std::unordered_map<std::pair<StringHash, StringHash>, std::shared_ptr<ShaderProgram>, VariationMapHasher>;

	public:
		bool BeginLoad(Stream& source) override;

		// Create and return a shader program with defines.
		// Existing program is returned if possible.
		// Variations should be cached to avoid repeated query.
		std::shared_ptr<ShaderProgram> CreateProgram(const std::string& vsDefines, const std::string& fsDefines);

	private:
		// Split the defines into a vector.
		void CreateDefinesVector(const std::string& defines, std::vector<std::string>& outVector);
		// Set shader code.
		// All existing variations are destroyed.
		void SetShaderCode(const std::string& source);
		// Parse includes in a shader code.
		void ProcessIncludes(const std::string& source, std::string& outResult);

		// NOTE: Changing shader code after ShaderPrograms were created will not reload materials that were using them.
		// For that an event will need to be used to notify existing materials that a shader was reloaded.
		// Thats due to the fact materials store a strong reference to the ShaderProgram.
		// However, mutating shader code during runtime is rather rare, so this is not implemented.

	private:
		// Shader source code.
		std::string sourceCode;
		// Shader programs.
		VariationMap programs;
	};
}
