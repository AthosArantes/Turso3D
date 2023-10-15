#pragma once

#include <Turso3D/Core/Object.h>
#include <Turso3D/IO/Stream.h>
#include <Turso3D/Utils/StringHash.h>
#include <unordered_map>
#include <memory>
#include <vector>

namespace Turso3D
{
	class Resource;
	class Shader;

	// Resource cache subsystem.
	// Loads resources on demand and stores them for later access.
	class ResourceCache : public Object
	{
		struct ResourceMapHasher
		{
			size_t operator()(const std::pair<RTTI::typeid_t, StringHash>& value) const noexcept
			{
				return (size_t)value.first ^ ((size_t)value.first + 0x9e3779b9 + (size_t)value.second);
			}
		};
		using ResourceMap = std::unordered_map<std::pair<RTTI::typeid_t, StringHash>, std::shared_ptr<Resource>, ResourceMapHasher>;

		using ShaderMap = std::unordered_map<StringHash, std::shared_ptr<Shader>>;

	public:
		// Construct and register subsystem and object types.
		ResourceCache();
		// Destruct.
		// Destroy all owned resources and unregister subsystem.
		~ResourceCache();

		RTTI_IMPL();

		// Add a resource directory.
		// Return true on success.
		bool AddResourceDir(const std::string& pathName, bool addFirst = false);
		// Remove a resource directory.
		void RemoveResourceDir(const std::string& pathName);

		// Open a resource data stream.
		// Return a pointer to the stream, or null if not found.
		std::unique_ptr<Stream> OpenResource(const std::string& name);

		// Load and return a resource.
		// The loaded resource will be stored in the cache.
		// NOTE: Do not load resources that uses GPU resources outside main thread.
		template <typename T, typename ...Args>
		auto LoadResource(const std::string& name, Args&&... args) -> std::enable_if_t<std::is_default_constructible_v<T>&& std::is_base_of_v<Resource, T>, std::shared_ptr<T>>
		{
			// Check if the resource was previously loaded
			auto k = std::make_pair(RTTI::GetTypeId<T>(), StringHash {name});

			if (auto it = resources.find(k); it != resources.end()) {
				return std::static_pointer_cast<T>(it->second);
			}

			std::unique_ptr<Stream> stream = OpenResource(name);
			if (stream) {
				std::shared_ptr<T> instance = std::make_shared<T>(std::forward<Args>(args)...);
				instance->SetName(name);

				if (instance->BeginLoad(*stream) && instance->EndLoad()) {
					auto iit = resources.insert(std::make_pair(k, std::static_pointer_cast<Resource>(instance)));
					assert(iit.second);
					return instance;
				}
			}

			return {};
		}

		// Create and load shader from a resource name.
		// Automatically add "_VS" or "_FS" suffix for the specified name.
		// e.g.: For "Shadow.glsl" it will load "Shadow_VS.glsl" and "Shadow_FS.glsl" source files for their shader stage.
		// This also uses an internal cache, so calling Load multiple times with same shader name will reuse the previously loaded shader.
		// Only call this from main thread.
		std::shared_ptr<Shader> LoadShader(const std::string& name);

		// Releases all resources that are only being kept alive by this cache.
		// Does not affect shaders.
		void ClearUnused();

	private:
		std::vector<std::string> resourceDirs;
		ResourceMap resources;
		ShaderMap shaders;
	};
}

RTTI_REGISTER(Turso3D::ResourceCache, Turso3D::Object);
