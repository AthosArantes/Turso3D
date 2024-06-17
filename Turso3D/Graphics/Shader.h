#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Resource/Resource.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

namespace Turso3D
{
	class Stream;
	class ShaderProgram;
	class ShaderPermutation;

	// Shader resource.
	// Stores shader source code, from which shader programs can be compiled & linked by specifying permutations (defines).
	class Shader : public Resource
	{
		struct LinkedProgram;

	public:
		Shader();
		~Shader();

		bool BeginLoad(Stream& source) override;

		// Create, compile and link a shader program.
		// Graphics must have been initialized.
		// Existing program is returned if possible.
		std::shared_ptr<ShaderProgram> Program(const ShaderPermutation& vsPermutation, const ShaderPermutation& fsPermutation);

	private:
		// Compile the shader.
		unsigned Compile(ShaderType type, const ShaderPermutation& permutation);

	private:
		// Explicit shader version.
		std::string version;
		// Shader code common to all stages.
		std::string sharedCode;
		// Shader code of a specific stage.
		std::string sourceCode[MAX_SHADER_TYPES];

		// Shader programs.
		// key is a combination of permutation hashes.
		std::unordered_map<size_t, LinkedProgram> programs;
	};
}
