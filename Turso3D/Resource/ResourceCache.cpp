#include "ResourceCache.h"
#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Graphics/Shader.h>
#include <Turso3D/IO/FileStream.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Resource/Image.h>
#include <Turso3D/Resource/Resource.h>
#include <Turso3D/Utils/StringUtils.h>
#include <filesystem>

namespace {
	// Must match the ShaderType enum
	const char* ShaderSuffix[] =
	{
		"_VS",
		"_FS",
		nullptr
	};
}

namespace Turso3D
{
	namespace fs = std::filesystem;

	// ==========================================================================================
	ResourceCache::ResourceCache()
	{
		RegisterSubsystem(this);
	}

	ResourceCache::~ResourceCache()
	{
		// Erase all programs from shaders
		for (auto it = shaders.begin(); it != shaders.end(); ++it) {
			it->second->programs.clear();
		}

		resources.clear();
		shaders.clear();
		RemoveSubsystem(this);
	}

	bool ResourceCache::AddResourceDir(const std::string& pathName, bool addFirst)
	{
		std::string path = (fs::current_path() / fs::path(pathName).make_preferred()).string();
		if (path.back() != fs::path::preferred_separator) {
			path += fs::path::preferred_separator;
		}

		// Check that the same path does not already exist
		for (const std::string& dir : resourceDirs) {
			if (dir == path) {
				return true;
			}
		}

		if (addFirst) {
			resourceDirs.insert(resourceDirs.begin(), path);
		} else {
			resourceDirs.push_back(path);
		}

		LOG_INFO("Added resource path \"{:s}\".", path);
		return true;
	}

	void ResourceCache::RemoveResourceDir(const std::string& pathName)
	{
		std::string path = (fs::current_path() / fs::path(pathName).make_preferred()).string();
		for (auto it = resourceDirs.begin(); it != resourceDirs.end(); ++it) {
			if (*it == path) {
				resourceDirs.erase(it);
				LOG_INFO("Removed resource path \"{:s}\".", path);
				return;
			}
		}
	}

	std::unique_ptr<Stream> ResourceCache::OpenResource(const std::string& name)
	{
		std::string _name = fs::path(name).make_preferred().string();

		std::unique_ptr<FileStream> stream = std::make_unique<FileStream>();
		for (const std::string& dir : resourceDirs) {
			if (stream->Open(dir + _name)) {
				return std::unique_ptr<Stream> {stream.release()};
			}
		}

		return {};
	}

	std::shared_ptr<Shader> ResourceCache::LoadShader(const std::string& name)
	{
		// Check if the shader has been loaded already
		StringHash hash {name};
		auto it = shaders.find(hash);
		if (it != shaders.end()) {
			return it->second;
		}

		fs::path filepath = fs::path {name};
		std::string filename {filepath.stem().string()};

		ResourceCache* cache = Object::Subsystem<ResourceCache>();
		assert(cache);

		// Load shader code for each shader type
		filepath.replace_filename(filename + ShaderSuffix[SHADER_VS]);
		std::unique_ptr<Stream> vs {cache->OpenResource(filepath.string() + ".glsl")};

		filepath.replace_filename(filename + ShaderSuffix[SHADER_FS]);
		std::unique_ptr<Stream> fs {cache->OpenResource(filepath.string() + ".glsl")};

		if (vs && fs) {
			std::shared_ptr<Shader> shader = std::make_shared<Shader>();
			shader->name = name;
			shader->SetShaderCode(SHADER_VS, *vs);
			shader->SetShaderCode(SHADER_FS, *fs);

			auto iit = shaders.insert(std::make_pair(hash, shader));
			assert(iit.second);

			return shader;
		}

		return {};
	}

	void ResourceCache::ClearUnused()
	{
		for (auto it = resources.begin(); it != resources.end(); ++it) {
			// use_count of 1 means only the cache is keeping it alive.
			if (it->second.use_count() == 1) {
				it = resources.erase(it);
			}
		}
	}
}
