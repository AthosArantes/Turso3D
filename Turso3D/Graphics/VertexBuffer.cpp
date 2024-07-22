#include <Turso3D/Graphics/VertexBuffer.h>
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/IO/Log.h>
#include <glew/glew.h>
#include <cassert>

namespace
{
	using namespace Turso3D;

	// Vertex element sizes by element type.
	static unsigned ElementTypeSize(ElementType value)
	{
		constexpr const unsigned data[] = {
			sizeof(int),
			sizeof(float),
			sizeof(Vector2),
			sizeof(Vector3),
			sizeof(Vector4),
			sizeof(unsigned),
			0
		};
		return data[value];
	}
}

namespace Turso3D
{
	VertexBuffer::VertexBuffer() :
		buffer(0),
		numVertices(0),
		vertexSize(0),
		usage(USAGE_DEFAULT),
		elementsHash(0)
	{
	}

	VertexBuffer::~VertexBuffer()
	{
		Release();
	}

	bool VertexBuffer::Define(ResourceUsage usage_, size_t numVertices_, const VertexElement* elements_, size_t numElements, const void* data)
	{
		Release();

		if (!numVertices_ || !numElements) {
			LOG_ERROR("Can not define vertex buffer with no vertices or no elements");
			return false;
		}

		numVertices = numVertices_;
		usage = usage_;

		// Determine offset of elements and the vertex size
		vertexSize = 0;
		elements.resize(numElements);
		for (size_t i = 0; i < numElements; ++i) {
			elements[i].type = elements_[i].type;
			elements[i].index = elements_[i].index;
			elements[i].normalized = elements_[i].normalized;
			elements[i].offset = vertexSize;
			vertexSize += ElementTypeSize(elements[i].type);
		}

		elementsHash = CalculateElementsHash(elements_, numElements);

		return Create(data);
	}

	bool VertexBuffer::SetData(size_t firstVertex, size_t numVertices_, const void* data, bool discard)
	{
		if (!data) {
			LOG_ERROR("Null source data for updating vertex buffer");
			return false;
		}
		if (firstVertex + numVertices_ > numVertices) {
			LOG_ERROR("Out of bounds range for updating vertex buffer");
			return false;
		}

		if (buffer) {
			glBindBuffer(GL_ARRAY_BUFFER, buffer);
			if (numVertices_ == numVertices) {
				glBufferData(GL_ARRAY_BUFFER, numVertices * vertexSize, data, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
			} else if (discard) {
				glBufferData(GL_ARRAY_BUFFER, numVertices * vertexSize, nullptr, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
				glBufferSubData(GL_ARRAY_BUFFER, firstVertex * vertexSize, numVertices_ * vertexSize, data);
			} else {
				glBufferSubData(GL_ARRAY_BUFFER, firstVertex * vertexSize, numVertices_ * vertexSize, data);
			}
		}

		return true;
	}

	bool VertexBuffer::Create(const void* data)
	{
		glGenBuffers(1, &buffer);
		if (!buffer) {
			LOG_ERROR("Failed to create vertex buffer");
			return false;
		}

		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glBufferData(GL_ARRAY_BUFFER, numVertices * vertexSize, data, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
		LOG_DEBUG("Created vertex buffer numVertices: {:d} vertexSize: {:d}", (unsigned)numVertices, vertexSize);

		return true;
	}

	void VertexBuffer::Release()
	{
		if (buffer) {
			Graphics::RemoveStateObject(this);
			glDeleteBuffers(1, &buffer);
			buffer = 0;
		}
	}

	// ==========================================================================================
	size_t VertexBuffer::CalculateElementsHash(const VertexElement* elements, size_t numElements)
	{
		size_t hash = 0;
		for (size_t i = 0; i < numElements; ++i) {
			hash ^= std::hash<size_t> {}((i << 24) | ((size_t)elements[i].normalized << 16) | ((size_t)elements[i].index << 8) | (size_t)elements[i].type);
		}
		return hash;
	}
}
