#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>

namespace Turso3D
{
	// GPU buffer for shader program uniform data.
	// Currently used for per-view camera parameters, Forward+ light data, skinning matrices and materials.
	// Not recommended to be used for small rapidly changing data like object's world matrix; bare uniforms will perform better.
	class UniformBuffer
	{
	public:
		// Construct.
		UniformBuffer();
		// Destruct.
		~UniformBuffer();

		// Define buffer with byte size.
		// Return true on success.
		bool Define(ResourceUsage usage, size_t size, const void* data = nullptr);
		// Redefine buffer data either completely or partially.
		// Return true on success.
		bool SetData(size_t offset, size_t numBytes, const void* data, bool discard = false);

		// Return size of buffer in bytes.
		size_t Size() const { return size; }
		// Return resource usage type.
		ResourceUsage Usage() const { return usage; }
		// Return whether is dynamic.
		bool IsDynamic() const { return usage == USAGE_DYNAMIC; }

		// Return the OpenGL object identifier.
		unsigned GLBuffer() const { return buffer; }

	private:
		// Create the GPU-side index buffer. Return true on success.
		bool Create(const void* data);
		// Release the index buffer and CPU shadow data.
		void Release();

	private:
		// OpenGL object identifier.
		unsigned buffer;
		// Buffer size in bytes.
		size_t size;
		// Resource usage type.
		ResourceUsage usage;
	};
}
