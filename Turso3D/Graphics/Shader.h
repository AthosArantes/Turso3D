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

	// Shader resource.
	// Defines shader source code, from which shader programs can be compiled & linked by specifying defines.
	class Shader : public Resource
	{
	public:
		Shader();
		~Shader();

		bool BeginLoad(Stream& source) override;

		// Create and return a shader program with defines.
		// Graphics must have been initialized.
		// Existing program is returned if possible.
		// Defines should be separated by space.
		// A define can have a value (Make sure there's no space around the equal sign).
		//	e.g.: SAMPLES=2
		std::shared_ptr<ShaderProgram> Program(const std::string& vsDefines, const std::string& fsDefines);

	private:
		// Compile the shader with defines.
		unsigned Compile(ShaderType type, const std::vector<std::string>& defines);

	private:
		// Explicit shader version.
		std::string version;
		// Shader code common to all stages.
		std::string sharedCode;
		// Shader code of a specific stage.
		std::string sourceCode[MAX_SHADER_TYPES];

		// Shader programs.
		// key is a combination hash of vs/fs defines.
		std::unordered_map<size_t, std::shared_ptr<ShaderProgram>> programs;
	};
}
