#pragma once

#include <Turso3D/Utils/StringHash.h>

namespace Turso3D
{
	class Stream;

	// Base class for resources.
	class Resource
	{
	public:
		virtual ~Resource() {}

		// Load the resource data from a stream.
		// May be executed outside the main thread, should not access GPU resources.
		// Return true on success.
		virtual bool BeginLoad(Stream& source);
		// Finish resource loading if necessary.
		// Always called from the main thread, so GPU resources can be accessed here.
		// Return true on success.
		virtual bool EndLoad();
		// Save the resource to a stream.
		// Return true on success.
		virtual bool Save(Stream& dest);

		// Load the resource synchronously from a binary stream.
		// Return true on success.
		bool Load(Stream& source);
		// Set name of the resource, usually the same as the file being loaded from.
		void SetName(const std::string& newName);

		// Return name of the resource.
		const std::string& Name() const { return name; }
		// Return name hash of the resource.
		const StringHash& NameHash() const { return nameHash; }

	private:
		// Resource name.
		std::string name;
		// Resource name hash.
		StringHash nameHash;
	};
}
