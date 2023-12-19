#pragma once

#include <Turso3D/IO/Stream.h>
#include <Turso3D/Utils/StringHash.h>
#include <limits.h>
#include <unordered_map>
#include <memory>
#include <vector>

namespace Turso3D
{
	class Resource;
	class Shader;

	// Resource cache, an independent subsystem.
	// Loads resources on demand and stores them for later access.
	class ResourceCache
	{
		ResourceCache();
		ResourceCache(const ResourceCache&);

	public:
		~ResourceCache();

		// Add a resource directory.
		// Return true on success.
		bool AddResourceDir(const std::string& pathName, unsigned priority = UINT_MAX);
		// Remove a resource directory.
		void RemoveResourceDir(const std::string& pathName);

		// Open a data stream.
		// Return a pointer to the stream, or null if not found.
		std::unique_ptr<Stream> OpenData(const std::string& name);

		// Load and return a resource.
		// The loaded resource will be stored in the cache.
		// NOTE: Do not load resources that uses GPU resources outside main thread.
		template <typename T, typename ...Args>
		auto LoadResource(const std::string& name, Args&&... args) -> std::enable_if_t<std::is_default_constructible_v<T>&& std::is_base_of_v<Resource, T>, std::shared_ptr<T>>
		{
			// Check if the resource was previously loaded
			StringHash hash {name};
			if (auto it = resources.find(hash); it != resources.end()) {
				return std::static_pointer_cast<T>(it->second);
			}

			std::unique_ptr<Stream> stream = OpenData(name);
			if (stream) {
				std::shared_ptr<T> resource = std::make_shared<T>(std::forward<Args>(args)...);
				resource->SetName(name);

				if (resource->BeginLoad(*stream) && resource->EndLoad()) {
					StoreResource<T>(resource);
					return resource;
				}
			}

			return {};
		}

		// Store a resource in the cache, it's name hash will be used as key.
		// Returns true if the resource was stored, false otherwise.
		template <typename T>
		auto StoreResource(const std::shared_ptr<T>& resource, bool replace = false) -> std::enable_if_t<std::is_base_of_v<Resource, T>, bool>
		{
			auto iit = resources.insert(std::make_pair(resource->NameHash(), std::static_pointer_cast<Resource>(resource)));
			if (iit.second) {
				return true;
			}
			if (!replace) {
				return false;
			}
			iit.first->second = std::static_pointer_cast<Resource>(resource);
			return true;
		}

		// Get an existing resource by it's name hash.
		template <typename T>
		auto GetResource(StringHash nameHash) -> std::enable_if_t<std::is_base_of_v<Resource, T>, std::shared_ptr<T>>
		{
			auto it = resources.find(nameHash);
			if (it != resources.end()) {
				return std::static_pointer_cast<T>(it->second);
			}
			return {};
		}
		// Get an existing resource by it's name hash.
		template <typename T>
		auto GetResource(const std::string& name) -> std::enable_if_t<std::is_base_of_v<Resource, T>, std::shared_ptr<T>>
		{
			return GetResource(StringHash {name});
		}

		// Releases all resources that are only being kept alive by this cache.
		void ClearUnused();

		// Get the ResourceCache instance.
		static ResourceCache* Instance();

	private:
		std::vector<std::string> resourceDirs;
		std::unordered_map<StringHash, std::shared_ptr<Resource>> resources;
	};
}
