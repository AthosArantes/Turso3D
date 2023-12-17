#include "ResourceCache.h"
#include "Image.h"
#include "Resource.h"
#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Graphics/Shader.h>
#include <Turso3D/IO/FileStream.h>
#include <Turso3D/IO/Log.h>
#include <algorithm>
#include <filesystem>

namespace Turso3D
{
	namespace fs = std::filesystem;

	// ==========================================================================================
	ResourceCache::ResourceCache()
	{
	}

	ResourceCache::~ResourceCache()
	{
		resources.clear();
	}

	bool ResourceCache::AddResourceDir(const std::string& pathName, unsigned priority)
	{
		std::error_code ec;
		fs::path base_path = fs::canonical(pathName, ec);

		if (ec) {
			LOG_ERROR("Failed to add resource directory \"{:s}\": {:s}", pathName, ec.message());
			return false;
		}

		if (!fs::is_directory(base_path)) {
			LOG_ERROR("The path \"{:s}\" is not a valid directory.", pathName);
			return false;
		}

		// Check that the same path does not already exist
		for (const std::string& dir : resourceDirs) {
			if (base_path == dir) {
				return true;
			}
		}

		if (priority != UINT_MAX) {
			resourceDirs.insert(resourceDirs.begin() + std::min(resourceDirs.size(), (size_t)priority), base_path.string());
		} else {
			resourceDirs.push_back(base_path.string());
		}

		LOG_INFO("Added resource path \"{:s}\".", base_path.string());
		return true;
	}

	void ResourceCache::RemoveResourceDir(const std::string& pathName)
	{
		for (auto it = resourceDirs.begin(); it != resourceDirs.end(); ++it) {
			if (*it == pathName) {
				resourceDirs.erase(it);
				LOG_INFO("Removed resource path \"{:s}\".", pathName);
				return;
			}
		}
	}

	std::unique_ptr<Stream> ResourceCache::OpenData(const std::string& name_)
	{
		// Try opening from file
		std::unique_ptr<FileStream> stream = std::make_unique<FileStream>();
		for (const std::string& dir : resourceDirs) {
			fs::path base_path {dir};

			std::error_code ec;
			fs::path filepath = fs::canonical(base_path / name_, ec);
			if (ec) {
				continue;
			}

			auto mm = std::mismatch(base_path.begin(), base_path.end(), filepath.begin());
			if (mm.first == base_path.end() && stream->Open(filepath.string())) {
				return std::unique_ptr<Stream> {stream.release()};
			}
		}

		return {};
	}

	void ResourceCache::ClearUnused()
	{
		for (auto it = resources.begin(); it != resources.end(); ) {
			// use_count of 1 means only the cache is keeping it alive.
			if (it->second.use_count() == 1) {
				it = resources.erase(it);
			} else {
				++it;
			}
		}
	}

	ResourceCache* ResourceCache::Instance()
	{
		static ResourceCache instance {};
		return &instance;
	}
}
