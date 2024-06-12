#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Utils/StringHash.h>
#include <vector>
#include <unordered_map>
#include <string>

namespace Turso3D
{
	class Vector2;
	class Vector3;
	class Vector4;
	class Matrix3;
	class Matrix3x4;
	class Matrix4;
	class Shader;

	// Linked shader program consisting of vertex and fragment shaders.
	class ShaderProgram
	{
		friend class Shader;

	public:
		// Construct
		ShaderProgram();
		// Destruct.
		~ShaderProgram();

		// Bind for using. No-op if already bound.
		// Return false if program is not successfully linked.
		bool Bind();

		// Return bitmask of used vertex attributes.
		unsigned Attributes() const { return attributes; }

		// Return uniform map.
		const std::unordered_map<StringHash, int>& Uniforms() const { return uniforms; }
		// Return uniform location by name or negative if not found.
		int Uniform(const std::string& name) const { return Uniform(StringHash {name}); }
		// Return uniform location by name or negative if not found.
		int Uniform(const char* name) const { return Uniform(StringHash {name}); }
		// Return uniform location by name hash or negative if not found.
		int Uniform(StringHash name) const
		{
			auto it = uniforms.find(name);
			return it != uniforms.end() ? it->second : -1;
		}
		// Return preset uniform location or negative if not found.
		int Uniform(PresetUniform uniform) const { return presetUniforms[uniform]; }

		// Set a float preset uniform.
		void SetUniform(PresetUniform uniform, float value);
		// Set a Vector2 preset uniform.
		void SetUniform(PresetUniform uniform, const Vector2& value);
		// Set a Vector3 preset uniform.
		void SetUniform(PresetUniform uniform, const Vector3& value);
		// Set a Vector4 preset uniform.
		void SetUniform(PresetUniform uniform, const Vector4& value);
		// Set a Matrix3 preset uniform.
		void SetUniform(PresetUniform uniform, const Matrix3& value);
		// Set a Matrix3x4 preset uniform.
		void SetUniform(PresetUniform uniform, const Matrix3x4& value);
		// Set a Matrix4 preset uniform.
		void SetUniform(PresetUniform uniform, const Matrix4& value);

		// Return the OpenGL shader program identifier.
		// Zero if not successfully compiled and linked.
		unsigned GLProgram() const { return program; }

	private:
		// Create a new shader program.
		// Graphics must have been initialized.
		bool Create(unsigned vs, unsigned fs);

		// Release the program.
		void Release();

	private:
		// OpenGL shader program identifier.
		unsigned program;

		// Used vertex attribute bitmask.
		unsigned attributes;
		// All uniform locations.
		std::unordered_map<StringHash, int> uniforms;
		// Preset uniform locations.
		int presetUniforms[MAX_PRESET_UNIFORMS];
	};
}
