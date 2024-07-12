#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <vector>

namespace Turso3D
{
	// GPU buffer for vertex data.
	class VertexBuffer
	{
		struct ElementInfo : VertexElement
		{
			// Element offset in the buffer.
			unsigned offset;
		};

	public:
		// Construct.
		VertexBuffer();
		// Destruct.
		~VertexBuffer();

		// Define buffer.
		// Return true on success.
		bool Define(ResourceUsage usage, size_t numVertices, const VertexElement* elements, size_t numElements, const void* data = nullptr);
		// Redefine buffer data either completely or partially.
		// Return true on success.
		bool SetData(size_t firstVertex, size_t numVertices, const void* data, bool discard = false);

		// Return number of vertices.
		size_t NumVertices() const { return numVertices; }
		// Return number of vertex elements.
		size_t NumElements() const { return elements.size(); }
		// Return a hash of the combination of all vertex elements.
		size_t ElementsHash() const { return elementsHash; }

		// Return vertex element.
		const VertexElement& GetElement(size_t index) const { return elements[index]; }
		// Return offset in buffer of the element.
		unsigned GetElementOffset(size_t index) const { return elements[index].offset; }

		// Return size of vertex in bytes.
		unsigned VertexSize() const { return vertexSize; }

		// Return resource usage type.
		ResourceUsage Usage() const { return usage; }
		// Return whether is dynamic.
		bool IsDynamic() const { return usage == USAGE_DYNAMIC; }

		// Return the OpenGL object identifier.
		unsigned GLBuffer() const { return buffer; }

		// Calculate a hash for all vertex elements.
		static size_t CalculateElementsHash(const VertexElement* elements, size_t numElements);

	private:
		// Create the GPU-side vertex buffer.
		// Return true on success.
		bool Create(const void* data);
		// Release the vertex buffer and CPU shadow data.
		void Release();

	private:
		// OpenGL object identifier.
		unsigned buffer;

		// Number of vertices.
		size_t numVertices;
		// Size of vertex in bytes.
		unsigned vertexSize;

		// Resource usage type.
		ResourceUsage usage;
		// Vertex elements.
		std::vector<ElementInfo> elements;
		// Vertex elements hash.
		size_t elementsHash;
	};
}
