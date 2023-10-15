#include "Material.h"
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/Graphics/UniformBuffer.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/IO/MemoryStream.h>
#include <Turso3D/Resource/ResourceCache.h>
#include <Turso3D/Utils/StringUtils.h>
#include <pugixml/pugixml.hpp>

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
	std::set<Material*> Material::allMaterials;
	std::shared_ptr<Material> Material::defaultMaterial;
	std::string Material::globalVSDefines;
	std::string Material::globalFSDefines;

	void Material::FreeDefaultMaterial()
	{
		defaultMaterial.reset();
	}

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
		if (loadedMaterial) {
			ResourceCache* cache = Subsystem<ResourceCache>();

			// Create passes
			for (auto& data : loadedMaterial->queuedPasses) {
				Pass* pass = CreatePass(data.type);
				pass->SetRenderState(data.blendMode, data.depthTest, data.colorWrite, data.depthWrite);
				pass->SetShader(cache->LoadShader(data.shader), data.vsDefines, data.fsDefines);
			}

			// Load textures
			for (auto& data : loadedMaterial->queuedTextures) {
				if (data.texture && data.texture->EndLoad()) {
					SetTexture(data.slot, data.texture);
				}
			}

			SetShaderDefines(loadedMaterial->vsDefines, loadedMaterial->fsDefines);

			loadedMaterial.reset();
			return true;
		}
		return false;
	}

	bool Material::LoadXML(pugi::xml_node& root)
	{
		using namespace pugi;

		loadedMaterial = std::make_unique<LoadedMaterialData>();

		if (xml_attribute attribute = root.attribute("vsDefines"); attribute) {
			loadedMaterial->vsDefines = attribute.value();
		}
		if (xml_attribute attribute = root.attribute("fsDefines"); attribute) {
			loadedMaterial->fsDefines = attribute.value();
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

					auto& data = loadedMaterial->queuedPasses.emplace_back();
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
			ResourceCache* cache = Subsystem<ResourceCache>();

			for (xml_node texture : node.children("texture")) {
				std::unique_ptr<Stream> image = cache->OpenResource(texture.attribute("name").value());
				if (!image) {
					continue;
				}

				auto& data = loadedMaterial->queuedTextures.emplace_back();
				data.slot = texture.attribute("slot").as_uint();

				data.texture = std::make_shared<Texture>();
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

	std::shared_ptr<Material> Material::Clone()
	{
		std::shared_ptr<Material> ret = std::make_shared<Material>();

		ret->cullMode = cullMode;

		for (size_t i = 0; i < MAX_PASS_TYPES; ++i) {
			Pass* pass = passes[i].get();
			if (pass) {
				Pass* clonePass = ret->CreatePass((PassType)i);
				clonePass->SetShader(pass->GetShader(), pass->VSDefines(), pass->FSDefines());
				clonePass->SetRenderState(pass->GetBlendMode(), pass->GetDepthTest(), pass->GetColorWrite(), pass->GetDepthWrite());
			}
		}

		for (size_t i = 0; i < MAX_MATERIAL_TEXTURE_UNITS; ++i) {
			ret->textures[i] = textures[i];
		}

		ret->uniformBuffer = uniformBuffer;
		ret->uniformValues = uniformValues;
		ret->uniformNameHashes = uniformNameHashes;
		ret->vsDefines = vsDefines;
		ret->fsDefines = fsDefines;

		return ret;
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

		for (auto it = allMaterials.begin(); it != allMaterials.end(); ++it) {
			Material* material = *it;
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

	const std::shared_ptr<Material>& Material::DefaultMaterial()
	{
		ResourceCache* cache = Subsystem<ResourceCache>();

		// Create on demand
		if (!defaultMaterial) {
			defaultMaterial = std::make_shared<Material>();

			std::vector<std::pair<std::string, Vector4>> defaultUniforms;
			defaultUniforms.push_back(std::make_pair("matDiffColor", Vector4::ONE));
			defaultUniforms.push_back(std::make_pair("matSpecColor", Vector4(0.25f, 0.25f, 0.25f, 1.0f)));
			defaultMaterial->DefineUniforms(defaultUniforms);

			Pass* pass = defaultMaterial->CreatePass(PASS_SHADOW);
			pass->SetShader(cache->LoadShader("Shaders/Shadow.glsl"), "", "");
			pass->SetRenderState(BLEND_REPLACE, CMP_LESS_EQUAL, false, true);

			pass = defaultMaterial->CreatePass(PASS_OPAQUE);
			pass->SetShader(cache->LoadShader("Shaders/NoTexture.glsl"), "", "");
			pass->SetRenderState(BLEND_REPLACE, CMP_LESS_EQUAL, true, true);
		}

		return defaultMaterial;
	}
}
