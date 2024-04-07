#include <Turso3D/IO/MemoryStream.h>
#include <cstring>

namespace Turso3D
{
	MemoryStream::MemoryStream()
	{
	}

	MemoryStream::MemoryStream(const std::vector<unsigned char>& data)
	{
		SetData(data);
	}

	MemoryStream::MemoryStream(const void* data, size_t numBytes)
	{
		SetData(data, numBytes);
	}

	MemoryStream::MemoryStream(Stream& source, size_t numBytes)
	{
		SetData(source, numBytes);
	}

	size_t MemoryStream::Read(void* dest, size_t numBytes)
	{
		if (numBytes + position > size) {
			numBytes = size - position;
		}
		if (!numBytes) {
			return 0;
		}

		unsigned char* srcPtr = &buffer[position];
		unsigned char* destPtr = (unsigned char*)dest;
		position += numBytes;

		size_t copySize = numBytes;
		while (copySize >= sizeof(unsigned)) {
			*((unsigned*)destPtr) = *((unsigned*)srcPtr);
			srcPtr += sizeof(unsigned);
			destPtr += sizeof(unsigned);
			copySize -= sizeof(unsigned);
		}
		if (copySize & sizeof(unsigned short)) {
			*((unsigned short*)destPtr) = *((unsigned short*)srcPtr);
			srcPtr += sizeof(unsigned short);
			destPtr += sizeof(unsigned short);
		}
		if (copySize & 1) {
			*destPtr = *srcPtr;
		}

		return numBytes;
	}

	size_t MemoryStream::Write(const void* data, size_t numBytes)
	{
		if (!numBytes) {
			return 0;
		}

		// Expand the buffer if necessary
		if (numBytes + position > size) {
			size = position + numBytes;
			buffer.resize(size);
		}

		unsigned char* srcPtr = (unsigned char*)data;
		unsigned char* destPtr = &buffer[position];
		position += numBytes;

		size_t copySize = numBytes;
		while (copySize >= sizeof(unsigned)) {
			*((unsigned*)destPtr) = *((unsigned*)srcPtr);
			srcPtr += sizeof(unsigned);
			destPtr += sizeof(unsigned);
			copySize -= sizeof(unsigned);
		}
		if (copySize & sizeof(unsigned short)) {
			*((unsigned short*)destPtr) = *((unsigned short*)srcPtr);
			srcPtr += sizeof(unsigned short);
			destPtr += sizeof(unsigned short);
		}
		if (copySize & 1) {
			*destPtr = *srcPtr;
		}

		return numBytes;
	}

	size_t MemoryStream::Seek(size_t newPosition)
	{
		if (newPosition > size) {
			newPosition = size;
		}
		position = newPosition;
		return position;
	}

	bool MemoryStream::IsReadable() const
	{
		return true;
	}

	bool MemoryStream::IsWritable() const
	{
		return true;
	}

	void MemoryStream::SetData(const std::vector<unsigned char>& data)
	{
		buffer = data;
		position = 0;
		size = data.size();
	}

	void MemoryStream::SetData(const void* data, size_t numBytes)
	{
		if (!data) {
			numBytes = 0;
		}

		buffer.resize(numBytes);
		if (numBytes) {
			memcpy(&buffer[0], data, numBytes);
		}

		position = 0;
		size = numBytes;
	}

	void MemoryStream::SetData(Stream& source, size_t numBytes)
	{
		buffer.resize(numBytes);
		size_t actualSize = source.Read(&buffer[0], numBytes);
		if (actualSize != numBytes) {
			buffer.resize(actualSize);
		}
		position = 0;
		size = actualSize;
	}

	void MemoryStream::Clear()
	{
		buffer.clear();
		position = 0;
		size = 0;
	}

	void MemoryStream::Resize(size_t newSize)
	{
		buffer.resize(newSize);
		size = newSize;
		if (position > size) {
			position = size;
		}
	}
}
