#include "Material.h"
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/Graphics/UniformBuffer.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/IO/MemoryStream.h>
#include <Turso3D/Resource/ResourceCache.h>
#include <pugixml/pugixml.hpp>
#include <set>
#include <filesystem>

namespace Turso3D
{
	const char* PassTypeNames[] = {
		"shadow",
		"opaque",
		"alpha",
		nullptr
	};

	const char* GeometryDefines[] =
	{
		"",
		"SKINNED ",
		"INSTANCED ",
		"",
		nullptr
	};

	// ==========================================================================================
	static std::set<Material*> allMaterials;

	std::string Material::globalVSDefines;
	std::string Material::globalFSDefines;

	// ==========================================================================================
	Pass::Pass(Material* parent) :
		parent(parent),
		blendMode(BLEND_REPLACE),
		depthTest(CMP_LESS_EQUAL),
		colorWrite(true),
		depthWrite(true)
	{
	}

	void Pass::SetShader(const std::shared_ptr<Shader>& shader_, const std::string& vsDefines_, const std::string& fsDefines_)
	{
		shader = shader_;
		vsDefines = vsDefines_;
		fsDefines = fsDefines_;

		if (vsDefines.length()) {
			vsDefines += ' ';
		}
		if (fsDefines.length()) {
			fsDefines += ' ';
		}

		ResetShaderPrograms();
	}

	void Pass::SetRenderState(BlendMode blendMode_, CompareMode depthTest_, bool colorWrite_, bool depthWrite_)
	{
		blendMode = blendMode_;
		depthTest = depthTest_;
		colorWrite = colorWrite_;
		depthWrite = depthWrite_;
	}

	void Pass::ResetShaderPrograms()
	{
		for (size_t i = 0; i < MAX_SHADER_VARIATIONS; ++i) {
			shaderPrograms[i].reset();
		}
	}

	// ==========================================================================================
	Material::Material() :
		cullMode(CULL_BACK),
		uniformsDirty(false)
	{
		allMaterials.insert(this);
	}

	Material::~Material()
	{
		allMaterials.erase(this);
	}

	bool Material::BeginLoad(Stream& source)
	{
		{
			MemoryStream buffer {};
			buffer.Resize(source.Size());
			buffer.Resize(source.Read(buffer.ModifiableData(), buffer.Size()));

			pugi::xml_document document {};
			pugi::xml_parse_result result = document.load_buffer(buffer.Data(), buffer.Size());
			if (result.status != pugi::status_ok) {
				LOG_ERROR("Failed to parse xml from archive \"{:s}\": {:s}", source.Name(), result.description());
				return false;
			}
			pugi::xml_node root = document.root().child("material");
			return LoadXML(root);
		}
		return false;
	}

	bool Material::EndLoad()
	{
		if (!loadBuffer) {
			return false;
		}

		ResourceCache* cache = ResourceCache::Instance();

		// Create passes
		for (auto& data : loadBuffer->passes) {
			Pass* pass = CreatePass(data.type);
			pass->SetRenderState(data.blendMode, data.depthTest, data.colorWrite, data.depthWrite);
			pass->SetShader(cache->LoadResource<Shader>(data.shader), data.vsDefines, data.fsDefines);
		}

		// Load textures
		for (auto& data : loadBuffer->textures) {
			if (data.texture && data.texture->EndLoad()) {
				SetTexture(data.slot, data.texture);
			}
		}

		SetShaderDefines(loadBuffer->vsDefines, loadBuffer->fsDefines);

		loadBuffer.reset();
		return true;
	}

	bool Material::LoadXML(pugi::xml_node& root)
	{
		using namespace pugi;

		loadBuffer = std::make_unique<MaterialLoadBuffer>();

		if (xml_attribute attribute = root.attribute("vsDefines"); attribute) {
			loadBuffer->vsDefines = attribute.value();
		}
		if (xml_attribute attribute = root.attribute("fsDefines"); attribute) {
			loadBuffer->fsDefines = attribute.value();
		}

		// Cull mode
		cullMode = CULL_BACK;
		if (xml_attribute attribute = root.attribute("cullMode"); attribute) {
			for (int i = 0; i < MAX_CULL_MODES; ++i) {
				std::string value {attribute.value()};
				if (value == CullModeNames[i]) {
					cullMode = (CullMode)i;
					break;
				}
			}
		}

		if (xml_node node = root.child("passes"); node) {
			for (xml_node pass : node.children()) {
				for (int t = 0; t < MAX_PASS_TYPES; ++t) {
					std::string passName {pass.name()};
					if (passName != PassTypeNames[t]) {
						continue;
					}

					auto& data = loadBuffer->passes.emplace_back();
					data.type = (PassType)t;
					data.blendMode = BLEND_REPLACE;
					data.depthTest = CMP_LESS_EQUAL;
					data.colorWrite = pass.attribute("colorWrite").as_bool(true);
					data.depthWrite = pass.attribute("depthWrite").as_bool(true);
					data.shader = pass.attribute("shader").value();
					data.vsDefines = pass.attribute("vsDefines").value();
					data.fsDefines = pass.attribute("fsDefines").value();

					if (xml_attribute attribute = pass.attribute("blendMode"); attribute) {
						for (int i = 0; i < MAX_BLEND_MODES; ++i) {
							std::string value {attribute.value()};
							if (value == BlendModeNames[i]) {
								data.blendMode = (BlendMode)i;
								break;
							}
						}
					}

					if (xml_attribute attribute = pass.attribute("depthTest"); attribute) {
						for (int i = 0; i < MAX_COMPARE_MODES; ++i) {
							std::string value {attribute.value()};
							if (value == CompareModeNames[i]) {
								data.depthTest = (CompareMode)i;
								break;
							}
						}
					}

					break;
				}
			}
		}

		if (xml_node node = root.child("textures"); node) {
			ResourceCache* cache = ResourceCache::Instance();

			// Material names containing a directory separator will be used as base path for texture loading.
			bool nameIsPath = Name().find_first_of("/\\") != std::string::npos;

			for (xml_node texture : node.children("texture")) {
				std::unique_ptr<Stream> image;

				std::string texname {texture.attribute("name").value()};

				// Force absolute if it begins with a forward slash
				if (texname.front() == '/') {
					image = cache->OpenData(texname.substr(1));

				} else if (nameIsPath) {
					image = cache->OpenData(std::filesystem::path {Name()}.replace_filename(texname).string());
					if (!image) {
						// Try again with texture name being absolute.
						image = cache->OpenData(texname);
					}

				} else {
					image = cache->OpenData(texname);
				}

				if (!image) {
					continue;
				}

				auto& data = loadBuffer->textures.emplace_back();
				data.slot = texture.attribute("slot").as_uint();
				bool srgb = texture.attribute("srgb").as_bool();

				data.texture = std::make_shared<Texture>();
				data.texture->SetLoadSRGB(srgb);
				if (!data.texture->BeginLoad(*image)) {
					data.texture.reset();
				}
			}
		}

		if (xml_node node = root.child("uniforms"); node) {
			std::vector<std::pair<std::string, Vector4>> newUniforms;
			for (xml_node uniform : node.children("uniform")) {
				newUniforms.emplace_back(
					std::make_pair(std::string(uniform.attribute("name").value()), Vector4(uniform.attribute("value").value()))
				);
			}
			DefineUniforms(newUniforms);
		}

		return true;
	}

	std::shared_ptr<Material> Material::Clone() const
	{
		std::shared_ptr<Material> mtl = std::make_shared<Material>();

		mtl->SetName(Name());
		mtl->cullMode = cullMode;

		for (size_t i = 0; i < MAX_PASS_TYPES; ++i) {
			Pass* pass = passes[i].get();
			if (pass) {
				Pass* clonePass = mtl->CreatePass((PassType)i);
				clonePass->SetShader(pass->GetShader(), pass->VSDefines(), pass->FSDefines());
				clonePass->SetRenderState(pass->GetBlendMode(), pass->GetDepthTest(), pass->GetColorWrite(), pass->GetDepthWrite());
			}
		}

		for (size_t i = 0; i < MAX_MATERIAL_TEXTURE_UNITS; ++i) {
			mtl->textures[i] = textures[i];
		}

		mtl->uniformBuffer = uniformBuffer;
		mtl->uniformValues = uniformValues;
		mtl->uniformNameHashes = uniformNameHashes;
		mtl->vsDefines = vsDefines;
		mtl->fsDefines = fsDefines;

		return mtl;
	}

	Pass* Material::CreatePass(PassType type)
	{
		if (!passes[type]) {
			passes[type] = std::make_shared<Pass>(this);
		}
		return passes[type].get();
	}

	void Material::RemovePass(PassType type)
	{
		passes[type].reset();
	}

	void Material::SetTexture(size_t index, const std::shared_ptr<Texture>& texture)
	{
		if (index < MAX_MATERIAL_TEXTURE_UNITS) {
			textures[index] = texture;
		}
	}

	void Material::ResetTextures()
	{
		for (size_t i = 0; i < MAX_MATERIAL_TEXTURE_UNITS; ++i) {
			textures[i].reset();
		}
	}

	void Material::SetShaderDefines(const std::string& vsDefines_, const std::string& fsDefines_)
	{
		vsDefines = vsDefines_;
		fsDefines = fsDefines_;
		if (vsDefines.length()) {
			vsDefines += ' ';
		}
		if (fsDefines.length()) {
			fsDefines += ' ';
		}

		for (size_t i = 0; i < MAX_PASS_TYPES; ++i) {
			if (passes[i]) {
				passes[i]->ResetShaderPrograms();
			}
		}
	}

	void Material::DefineUniforms(size_t numUniforms, const char** uniformNames)
	{
		uniformNameHashes.resize(numUniforms);
		uniformValues.resize(numUniforms);
		for (size_t i = 0; i < numUniforms; ++i) {
			uniformNameHashes[i] = StringHash(uniformNames[i]);
		}
		uniformsDirty = true;
	}

	void Material::DefineUniforms(const std::vector<std::string>& uniformNames)
	{
		uniformNameHashes.resize(uniformNames.size());
		uniformValues.resize(uniformNames.size());
		for (size_t i = 0; i < uniformNames.size(); ++i) {
			uniformNameHashes[i] = StringHash(uniformNames[i]);
		}
		uniformsDirty = true;
	}

	void Material::DefineUniforms(const std::vector<std::pair<std::string, Vector4>>& uniforms)
	{
		uniformValues.resize(uniforms.size());
		uniformNameHashes.resize(uniforms.size());
		for (size_t i = 0; i < uniforms.size(); ++i) {
			uniformNameHashes[i] = StringHash(uniforms[i].first);
			uniformValues[i] = uniforms[i].second;
		}
		uniformsDirty = true;
	}

	void Material::SetUniform(size_t index, const Vector4& value)
	{
		if (index >= uniformValues.size()) {
			return;
		}
		uniformValues[index] = value;
		uniformsDirty = true;
	}

	void Material::SetUniform(const std::string& name_, const Vector4& value)
	{
		SetUniform(StringHash(name_), value);
	}

	void Material::SetUniform(const char* name_, const Vector4& value)
	{
		SetUniform(StringHash(name_), value);
	}

	void Material::SetUniform(StringHash nameHash_, const Vector4& value)
	{
		for (size_t i = 0; i < uniformNameHashes.size(); ++i) {
			if (uniformNameHashes[i] == nameHash_) {
				uniformValues[i] = value;
				uniformsDirty = true;
				return;
			}
		}
	}

	void Material::SetGlobalShaderDefines(const std::string& vsDefines_, const std::string& fsDefines_)
	{
		globalVSDefines = vsDefines_;
		globalFSDefines = fsDefines_;
		if (globalVSDefines.length()) {
			globalVSDefines += ' ';
		}
		if (globalFSDefines.length()) {
			globalFSDefines += ' ';
		}

		for (Material* material : allMaterials) {
			for (size_t i = 0; i < MAX_PASS_TYPES; ++i) {
				if (material->passes[i]) {
					material->passes[i]->ResetShaderPrograms();
				}
			}
		}
	}

	void Material::SetCullMode(CullMode mode)
	{
		cullMode = mode;
	}

	UniformBuffer* Material::GetUniformBuffer() const
	{
		if (uniformsDirty) {
			// If is a shared uniform buffer from clone operation, make unique now
			if (!uniformBuffer || uniformBuffer.use_count() > 1) {
				uniformBuffer = std::make_shared<UniformBuffer>();
			}

			if (uniformValues.size()) {
				if (uniformBuffer->Size() != uniformValues.size() * sizeof(Vector4)) {
					uniformBuffer->Define(USAGE_DEFAULT, uniformValues.size() * sizeof(Vector4), &uniformValues[0]);
				} else {
					uniformBuffer->SetData(0, uniformValues.size() * sizeof(Vector4), &uniformValues[0]);
				}
			}

			uniformsDirty = false;
		}
		return uniformBuffer.get();
	}

	const Vector4& Material::Uniform(const std::string& name_) const
	{
		return Uniform(StringHash(name_));
	}

	const Vector4& Material::Uniform(const char* name_) const
	{
		return Uniform(StringHash(name_));
	}

	const Vector4& Material::Uniform(StringHash nameHash_) const
	{
		for (size_t i = 0; i < uniformNameHashes.size(); ++i) {
			if (uniformNameHashes[i] == nameHash_) {
				return uniformValues[i];
			}
		}
		return Vector4::ZERO;
	}

	std::shared_ptr<Material> Material::GetDefault()
	{
		constexpr const char* mtlName = "__defaultMaterial";
		constexpr StringHash defaultNameHash {mtlName};

		ResourceCache* cache = ResourceCache::Instance();

		std::shared_ptr<Material> mtl = cache->GetResource<Material>(defaultNameHash);
		if (!mtl) {
			mtl = std::make_shared<Material>();
			mtl->SetName(mtlName);

			std::vector<std::pair<std::string, Vector4>> defaultUniforms;
			defaultUniforms.push_back(std::make_pair("matDiffColor", Vector4::ONE));
			defaultUniforms.push_back(std::make_pair("matSpecColor", Vector4 {0.04f, 0.04f, 0.04f, 0.0f}));
			mtl->DefineUniforms(defaultUniforms);

			Pass* pass = mtl->CreatePass(PASS_SHADOW);
			pass->SetShader(cache->LoadResource<Shader>("Shadow.glsl"), "", "");
			pass->SetRenderState(BLEND_REPLACE, CMP_LESS_EQUAL, false, true);

			pass = mtl->CreatePass(PASS_OPAQUE);
			pass->SetShader(cache->LoadResource<Shader>("NoTexture.glsl"), "", "");
			pass->SetRenderState(BLEND_REPLACE, CMP_LESS_EQUAL, true, true);

			cache->StoreResource(mtl);
		}
		return mtl;
	}
}
