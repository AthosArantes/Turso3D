#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Utils/StringHash.h>
#include <vector>
#include <map>
#include <string>

namespace Turso3D
{
	// Linked shader program consisting of vertex and fragment shaders.
	class ShaderProgram
	{
	public:
		// Construct
		ShaderProgram();
		// Destruct.
		~ShaderProgram();

		// Compile & link from shader source code and defines.
		// Graphics subsystem must have been initialized.
		bool Create(const std::string& code, const std::vector<std::string>& vsDefines, const std::vector<std::string>& fsDefines);

		// Bind for using. No-op if already bound.
		// Return false if program is not successfully linked.
		bool Bind();

		// Set shader program name
		void SetName(const std::string& newName);
		// Return shader program name.
		const std::string& Name() const { return name; }

		// Return bitmask of used vertex attributes.
		unsigned Attributes() const { return attributes; }
		// Return uniform map.
		const std::map<StringHash, int>& Uniforms() const { return uniforms; }
		// Return uniform location by name or negative if not found.
		int Uniform(const std::string& name) const;
		// Return uniform location by name or negative if not found.
		int Uniform(const char* name) const;
		// Return uniform location by name hash or negative if not found.
		int Uniform(StringHash name) const;
		// Return preset uniform location or negative if not found.
		int Uniform(PresetUniform uniform) const { return presetUniforms[uniform]; }

		// Return the OpenGL shader program identifier.
		// Zero if not successfully compiled and linked.
		unsigned GLProgram() const { return program; }

	private:
		// Compile the shader.
		unsigned CompileShader(ShaderType type, const std::string& code, const std::vector<std::string>& defines);
		// Release the program.
		void Release();

	private:
		// Shader program name.
		std::string name;
		// OpenGL shader program identifier.
		unsigned program;
		// Used vertex attribute bitmask.
		unsigned attributes;
		// All uniform locations.
		std::map<StringHash, int> uniforms;
		// Preset uniform locations.
		int presetUniforms[MAX_PRESET_UNIFORMS];
	};
}
