#pragma once

#include <Turso3D/IO/Stream.h>

namespace Turso3D
{
	// File open mode.
	enum FileMode
	{
		FILE_READ = 0,
		FILE_READWRITE,
		FILE_READWRITE_TRUNCATE
	};

	// Filesystem file.
	class FileStream : public Stream
	{
	public:
		// Construct.
		FileStream();
		// Construct and open a file.
		FileStream(const std::string& fileName, FileMode fileMode = FILE_READ);
		// Destruct.
		// Close the file if open.
		~FileStream();

		// Read bytes from the file.
		// Return number of bytes actually read.
		size_t Read(void* dest, size_t numBytes) override;
		// Write bytes to the file.
		// Return number of bytes actually written.
		size_t Write(const void* data, size_t numBytes) override;
		// Set position in bytes from the beginning of the file.
		size_t Seek(size_t newPosition) override;

		// Return whether read operations are allowed.
		bool IsReadable() const override;
		// Return whether write operations are allowed.
		bool IsWritable() const override;

		// Open a file. Return true on success.
		bool Open(const std::string& fileName, FileMode fileMode = FILE_READ);
		// Close the file.
		void Close();
		// Flush any buffered output to the file.
		void Flush();

		// Return the open mode.
		FileMode Mode() const { return mode; }
		// Return whether is open.
		bool IsOpen() const;
		// Return the file handle.
		void* Handle() const { return handle; }

		using Stream::Read;
		using Stream::Write;

	private:
		// Open mode.
		FileMode mode;
		// File handle.
		void* handle;
		// Synchronization needed before read -flag.
		bool readSyncNeeded;
		// Synchronization needed before write -flag.
		bool writeSyncNeeded;
	};
}
