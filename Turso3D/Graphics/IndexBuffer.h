#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>

namespace Turso3D
{
	// GPU buffer for index data.
	class IndexBuffer
	{
	public:
		// Construct.
		IndexBuffer();
		// Destruct.
		~IndexBuffer();

		// Define buffer.
		// Return true on success.
		bool Define(ResourceUsage usage, size_t numIndices, size_t indexSize, const void* data = nullptr);
		// Redefine buffer data either completely or partially.
		// Return true on success.
		bool SetData(size_t firstIndex, size_t numIndices, const void* data, bool discard = false);

		// Return number of indices.
		size_t NumIndices() const { return numIndices; }
		// Return size of index in bytes.
		size_t IndexSize() const { return indexSize; }
		// Return resource usage type.
		ResourceUsage Usage() const { return usage; }
		// Return whether is dynamic.
		bool IsDynamic() const { return usage == USAGE_DYNAMIC; }

		// Return the OpenGL object identifier.
		unsigned GLBuffer() const { return buffer; }

	private:
		// Create the GPU-side index buffer.
		// Return true on success.
		bool Create(const void* data);
		// Release the index buffer and CPU shadow data.
		void Release();

	private:
		// OpenGL object identifier.
		unsigned buffer;
		// Number of indices.
		size_t numIndices;
		// Size of index in bytes.
		size_t indexSize;
		// Resource usage type.
		ResourceUsage usage;
	};
}
