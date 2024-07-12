#include <Turso3D/Graphics/UniformBuffer.h>
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/IO/Log.h>
#include <glew/glew.h>
#include <cstring>
#include <cassert>

namespace Turso3D
{
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
			Graphics::RemoveStateObject(this);
			glDeleteBuffers(1, &buffer);
			buffer = 0;
		}
	}
}
