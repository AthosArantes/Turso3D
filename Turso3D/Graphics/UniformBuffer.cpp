#include "UniformBuffer.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/IO/Log.h>
#include <glew/glew.h>
#include <cstring>
#include <cassert>

namespace Turso3D
{
	static UniformBuffer* boundUniformBuffers[MAX_CONSTANT_BUFFER_SLOTS];

	UniformBuffer::UniformBuffer() :
		buffer(0),
		size(0),
		usage(USAGE_DEFAULT)
	{
	}

	UniformBuffer::~UniformBuffer()
	{
		Release();
	}

	bool UniformBuffer::Define(ResourceUsage usage_, size_t size_, const void* data)
	{
		Release();

		if (!size_) {
			LOG_ERROR("Can not define empty uniform buffer");
			return false;
		}

		size = size_;
		usage = usage_;

		return Create(data);
	}

	bool UniformBuffer::SetData(size_t offset, size_t numBytes, const void* data, bool discard)
	{
		if (!numBytes) {
			return true;
		}

		if (!data) {
			LOG_ERROR("Null source data for updating uniform buffer");
			return false;
		}
		if (offset + numBytes > size) {
			LOG_ERROR("Out of bounds range for updating uniform buffer");
			return false;
		}

		if (buffer) {
			glBindBuffer(GL_UNIFORM_BUFFER, buffer);
			if (numBytes == size) {
				glBufferData(GL_UNIFORM_BUFFER, numBytes, data, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
			} else if (discard) {
				glBufferData(GL_UNIFORM_BUFFER, size, nullptr, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
				glBufferSubData(GL_UNIFORM_BUFFER, offset, numBytes, data);
			} else {
				glBufferSubData(GL_UNIFORM_BUFFER, offset, numBytes, data);
			}
		}

		return true;
	}

	void UniformBuffer::Bind(size_t index)
	{
		if (!buffer || boundUniformBuffers[index] == this) {
			return;
		}
		glBindBufferRange(GL_UNIFORM_BUFFER, (GLuint)index, buffer, 0, size);
		boundUniformBuffers[index] = this;
	}

	void UniformBuffer::Unbind(size_t index)
	{
		if (boundUniformBuffers[index]) {
			glBindBufferRange(GL_UNIFORM_BUFFER, (GLuint)index, 0, 0, 0);
			boundUniformBuffers[index] = nullptr;
		}
	}

	bool UniformBuffer::Create(const void* data)
	{
		glGenBuffers(1, &buffer);
		if (!buffer) {
			LOG_ERROR("Failed to create uniform buffer");
			return false;
		}
		glBindBuffer(GL_UNIFORM_BUFFER, buffer);
		glBufferData(GL_UNIFORM_BUFFER, size, data, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
		LOG_DEBUG("Created constant buffer size {}", (unsigned)size);
		return true;
	}

	void UniformBuffer::Release()
	{
		if (buffer) {
			glDeleteBuffers(1, &buffer);
			buffer = 0;
			for (size_t i = 0; i < MAX_CONSTANT_BUFFER_SLOTS; ++i) {
				if (boundUniformBuffers[i] == this) {
					boundUniformBuffers[i] = nullptr;
				}
			}
		}
	}
}
