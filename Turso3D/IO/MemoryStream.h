#pragma once

#include <Turso3D/IO/Stream.h>

namespace Turso3D
{
	// Dynamically sized buffer that can be read and written to as a stream.
	class MemoryStream : public Stream
	{
	public:
		// Construct an empty buffer.
		MemoryStream();
		// Construct from another buffer.
		MemoryStream(const std::vector<uint8_t>& data);
		// Construct from a memory area.
		MemoryStream(const void* data, size_t numBytes);
		// Construct from a stream.
		MemoryStream(Stream& source, size_t numBytes);

		// Read bytes from the buffer.
		// Return number of bytes actually read.
		size_t Read(void* dest, size_t size) override;
		// Write bytes to the buffer.
		// Return number of bytes actually written.
		size_t Write(const void* data, size_t size) override;
		// Set position in bytes from the beginning of the buffer.
		size_t Seek(size_t newPosition) override;

		// Return whether read operations are allowed.
		bool IsReadable() const override;
		// Return whether write operations are allowed.
		bool IsWritable() const override;

		// Set data from another buffer.
		void SetData(const std::vector<uint8_t>& data);
		// Set data from a memory area.
		void SetData(const void* data, size_t numBytes);
		// Set data from a stream.
		void SetData(Stream& source, size_t numBytes);
		// Reset to zero size.
		void Clear();
		// Set size.
		void Resize(size_t newSize);

		// Return data.
		const uint8_t* Data() const { return &*buffer.begin(); }
		// Return non-const data.
		uint8_t* ModifiableData() { return const_cast<uint8_t*>(&*buffer.begin()); }
		// Return the buffer.
		const std::vector<uint8_t>& Buffer() const { return buffer; }

		using Stream::Read;
		using Stream::Write;

	private:
		// Dynamic data buffer.
		std::vector<uint8_t> buffer;
	};
}
