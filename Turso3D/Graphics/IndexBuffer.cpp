#include <Turso3D/Graphics/IndexBuffer.h>
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/IO/Log.h>
#include <glew/glew.h>
#include <cassert>
#include <cstring>

namespace Turso3D
{
	static IndexBuffer* boundIndexBuffer = nullptr;
	static size_t boundIndexSize = 0;

	IndexBuffer::IndexBuffer() :
		buffer(0),
		numIndices(0),
		indexSize(0),
		usage(USAGE_DEFAULT)
	{
	}

	IndexBuffer::~IndexBuffer()
	{
		Release();
	}

	bool IndexBuffer::Define(ResourceUsage usage_, size_t numIndices_, size_t indexSize_, const void* data)
	{
		Release();

		if (!numIndices_) {
			LOG_ERROR("Can not define index buffer with no indices");
			return false;
		}
		if (indexSize_ != sizeof(unsigned) && indexSize_ != sizeof(unsigned short)) {
			LOG_ERROR("Index buffer index size must be 2 or 4");
			return false;
		}

		numIndices = numIndices_;
		indexSize = indexSize_;
		usage = usage_;

		return Create(data);
	}

	bool IndexBuffer::SetData(size_t firstIndex, size_t numIndices_, const void* data, bool discard)
	{
		if (!data) {
			LOG_ERROR("Null source data for updating index buffer");
			return false;
		}
		if (firstIndex + numIndices_ > numIndices) {
			LOG_ERROR("Out of bounds range for updating index buffer");
			return false;
		}

		if (buffer) {
			Bind();

			if (numIndices_ == numIndices) {
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * indexSize, data, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
			} else if (discard) {
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * indexSize, nullptr, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
				glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, firstIndex * indexSize, numIndices_ * indexSize, data);
			} else {
				glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, firstIndex * indexSize, numIndices_ * indexSize, data);
			}
		}

		return true;
	}

	void IndexBuffer::Bind()
	{
		if (!buffer || boundIndexBuffer == this) {
			return;
		}
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
		boundIndexBuffer = this;
		boundIndexSize = indexSize;
	}

	bool IndexBuffer::Create(const void* data)
	{
		glGenBuffers(1, &buffer);
		if (!buffer) {
			LOG_ERROR("Failed to create index buffer");
			return false;
		}

		Bind();

		glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * indexSize, data, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
		LOG_DEBUG("Created index buffer numIndices {} indexSize {}", (unsigned)numIndices, (unsigned)indexSize);

		return true;
	}

	void IndexBuffer::Release()
	{
		if (buffer) {
			glDeleteBuffers(1, &buffer);
			buffer = 0;
			if (boundIndexBuffer == this) {
				boundIndexBuffer = nullptr;
				boundIndexSize = 0;
			}
		}
	}

	// ==========================================================================================
	size_t IndexBuffer::BoundIndexSize()
	{
		return boundIndexSize;
	}
}
