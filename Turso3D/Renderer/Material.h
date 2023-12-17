#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Graphics/Shader.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Math/Vector4.h>
#include <Turso3D/Resource/Resource.h>
#include <memory>

namespace pugi
{
	class xml_node;
}

namespace Turso3D
{
	class Material;
	class Texture;
	class UniformBuffer;

	enum PassType
	{
		PASS_SHADOW = 0,
		PASS_OPAQUE,
		PASS_ALPHA,
		MAX_PASS_TYPES
	};
	extern const char* PassTypeNames[];

	extern const char* GeometryDefines[];
	//extern const char* lightDefines[];
	//extern const char* dirLightDefines[];

	// Shader program bits
	constexpr unsigned SP_STATIC = 0x0;
	constexpr unsigned SP_SKINNED = 0x1;
	constexpr unsigned SP_INSTANCED = 0x2;
	constexpr unsigned SP_CUSTOMGEOM = 0x3;
	constexpr unsigned SP_GEOMETRYBITS = 0x3;

	static const size_t MAX_SHADER_VARIATIONS = 4;

	// ==========================================================================================
	// Render pass, which defines render state and shaders.
	// A material may define several of these.
	class Pass
	{
	public:
		// Construct.
		Pass(Material* parent);

		// Set shader and shader defines.
		// Existing shader programs will be cleared.
		void SetShader(const std::shared_ptr<Shader>& shader, const std::string& vsDefines, const std::string& fsDefines);
		// Reset existing shader programs.
		void ResetShaderPrograms();
		// Set render state.
		void SetRenderState(BlendMode blendMode, CompareMode depthTest = CMP_LESS, bool colorWrite = true, bool depthWrite = true);
		// Get a shader program and cache for later use.
		ShaderProgram* GetShaderProgram(unsigned char programBits);

		// Return parent material.
		Material* Parent() const { return parent; }
		// Return shader.
		const std::shared_ptr<Shader>& GetShader() const { return shader; }
		// Return vertex shader defines.
		const std::string& VSDefines() const { return vsDefines; }
		// Return fragment shader defines.
		const std::string& FSDefines() const { return fsDefines; }
		// Return blend mode.
		BlendMode GetBlendMode() const { return blendMode; }
		// Return depth test mode.
		CompareMode GetDepthTest() const { return depthTest; }
		// Return color write flag.
		bool GetColorWrite() const { return colorWrite; }
		// Return depth write flag.
		bool GetDepthWrite() const { return depthWrite; }

	public:
		// Last sort key for combined distance and state sorting. Used by Renderer.
		std::pair<unsigned short, unsigned short> lastSortKey;

	private:
		// Parent material.
		Material* parent;
		// Blend mode.
		BlendMode blendMode;
		// Depth test mode.
		CompareMode depthTest;
		// Color write flag.
		bool colorWrite;
		// Depth write flag.
		bool depthWrite;
		// Cached shader variations.
		std::shared_ptr<ShaderProgram> shaderPrograms[MAX_SHADER_VARIATIONS];
		// Shader resource.
		std::shared_ptr<Shader> shader;
		// Vertex shader defines.
		std::string vsDefines;
		// Fragment shader defines.
		std::string fsDefines;
	};

	// Material resource, which describes how to render 3D geometry and refers to textures.
	// A material can contain several passes (for example normal rendering, and depth only.)
	class Material : public Resource
	{
	public:
		// Construct.
		Material();
		// Destruct.
		~Material();

		// Load material from a stream.
		// Return true on success.
		bool BeginLoad(Stream& source) override;
		// Finalize material loading in the main thread.
		// Return true on success.
		bool EndLoad() override;

		// Load from an xml node.
		bool LoadXML(pugi::xml_node& root);

		// Return a clone of the material.
		std::shared_ptr<Material> Clone() const;

		// Create and return a new pass.
		// If pass with same name exists, it will be returned.
		Pass* CreatePass(PassType type);
		// Remove a pass.
		void RemovePass(PassType type);

		// Set a texture.
		void SetTexture(size_t index, const std::shared_ptr<Texture>& texture);
		// Reset all texture assignments.
		void ResetTextures();

		// Set shader defines for all passes.
		void SetShaderDefines(const std::string& vsDefines, const std::string& fsDefines);

		// Define uniform buffer layout. All material uniforms are Vector4's for simplicity.
		void DefineUniforms(size_t numUniforms, const char** uniformNames);
		// Define uniform buffer layout.
		void DefineUniforms(const std::vector<std::string>& uniformNames);
		// Define uniform buffer layout with initial values.
		void DefineUniforms(const std::vector<std::pair<std::string, Vector4>>& uniforms);
		// Set an uniform value by index.
		void SetUniform(size_t index, const Vector4& value);
		// Set an uniform value by name.
		void SetUniform(const std::string& name, const Vector4& value);
		// Set an uniform value by name.
		void SetUniform(const char* name, const Vector4& value);
		// Set an uniform value by name hash.
		void SetUniform(StringHash nameHash, const Vector4& value);
		// Set culling mode, shared by all passes.
		void SetCullMode(CullMode mode);

		// Return pass by index or null if not found.
		Pass* GetPass(PassType type) const { return passes[type].get(); }
		// Return texture by texture unit.
		const std::shared_ptr<Texture>& GetTexture(size_t index) const { return textures[index]; }

		// Return the uniform buffer.
		// If the uniform buffer was not yet created, a new one will be created.
		// If the uniform buffer is shared and this material changes any of the uniform values,
		// a new uniform buffer will be created.
		UniformBuffer* GetUniformBuffer() const;

		// Return number of uniforms.
		size_t NumUniforms() const { return uniformValues.size(); }
		// Return uniform value by index.
		const Vector4& Uniform(size_t index) const { return uniformValues[index]; }
		// Return uniform value by name.
		const Vector4& Uniform(const std::string& name) const;
		// Return uniform value by name.
		const Vector4& Uniform(const char* name) const;
		// Return uniform value by name hash.
		const Vector4& Uniform(StringHash nameHash) const;
		// Return culling mode.
		CullMode GetCullMode() const { return cullMode; }

		// Return vertex shader defines.
		const std::string& VSDefines() const { return vsDefines; }
		// Return fragment shader defines.
		const std::string& FSDefines() const { return fsDefines; }

		// Return a default opaque untextured material.
		static std::shared_ptr<Material> GetDefault();

		// Set global (lighting-related) shader defines.
		// Resets all loaded pass shaders.
		static void SetGlobalShaderDefines(const std::string& vsDefines, const std::string& fsDefines);
		// Return global vertex shader defines.
		static const std::string& GlobalVSDefines() { return globalVSDefines; }
		// Return global fragment shader defines.
		static const std::string& GlobalFSDefines() { return globalFSDefines; }

	private:
		// Culling mode.
		CullMode cullMode;
		// Passes.
		std::shared_ptr<Pass> passes[MAX_PASS_TYPES];
		// Material textures.
		std::shared_ptr<Texture> textures[MAX_MATERIAL_TEXTURE_UNITS];
		// Uniform buffer.
		mutable std::shared_ptr<UniformBuffer> uniformBuffer;
		// Uniform name hashes.
		std::vector<StringHash> uniformNameHashes;
		// Uniform values.
		std::vector<Vector4> uniformValues;
		// Uniforms dirty flag.
		mutable bool uniformsDirty;

		// Vertex shader defines for all passes.
		std::string vsDefines;
		// Fragment shader defines for all passes.
		std::string fsDefines;

		// Represents material data that was read from file.
		struct MaterialLoadBuffer
		{
			struct PassBuffer
			{
				PassType type;
				BlendMode blendMode;
				CompareMode depthTest;
				bool colorWrite;
				bool depthWrite;
				std::string shader;
				std::string vsDefines;
				std::string fsDefines;
			};
			std::vector<PassBuffer> passes;

			struct TextureBuffer
			{
				unsigned slot;
				std::shared_ptr<Texture> texture;
			};
			std::vector<TextureBuffer> textures;

			std::string vsDefines;
			std::string fsDefines;
		};
		std::unique_ptr<MaterialLoadBuffer> loadBuffer;

		// Global vertex shader defines.
		static std::string globalVSDefines;
		// Global fragment shader defines.
		static std::string globalFSDefines;
	};

	// ==========================================================================================
	inline ShaderProgram* Pass::GetShaderProgram(unsigned char programBits)
	{
		if (shaderPrograms[programBits]) {
			return shaderPrograms[programBits].get();
		}

		if (!shader) {
			return nullptr;
		}

		unsigned char geomBits = programBits & SP_GEOMETRYBITS;
		std::shared_ptr<ShaderProgram> newShaderProgram = shader->CreateProgram(
			Material::GlobalVSDefines() + parent->VSDefines() + vsDefines + GeometryDefines[geomBits],
			Material::GlobalFSDefines() + parent->FSDefines() + fsDefines
		);
		shaderPrograms[programBits] = newShaderProgram;

		return newShaderProgram.get();
	}
}
