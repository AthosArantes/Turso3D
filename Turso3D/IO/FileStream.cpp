#include "FileStream.h"
#include <cstdio>

static const char* OpenModes[] =
{
	"rb",
	"r+b",
	"w+b"
};

namespace Turso3D
{
	FileStream::FileStream() :
		mode(FILE_READ),
		handle(nullptr),
		readSyncNeeded(false),
		writeSyncNeeded(false)
	{
	}

	FileStream::FileStream(const std::string& fileName, FileMode mode) :
		mode(FILE_READ),
		handle(nullptr),
		readSyncNeeded(false),
		writeSyncNeeded(false)
	{
		Open(fileName, mode);
	}

	FileStream::~FileStream()
	{
		Close();
	}

	bool FileStream::Open(const std::string& fileName, FileMode fileMode)
	{
		Close();

		if (fileName.empty()) {
			return false;
		}

		std::string filepath(fileName);

#ifdef _WIN32
		// Convert forward slashes to backslashes
		char* filepathData = filepath.data();
		for (size_t i = 0; i < filepath.size(); ++i) {
			if (filepathData[i] == '/') {
				filepathData[i] = '\\';
			}
		}
#endif
		handle = fopen(filepath.c_str(), OpenModes[fileMode]);

		// If file did not exist in readwrite mode, retry with truncate mode
		if (mode == FILE_READWRITE && !handle) {
			handle = fopen(filepath.c_str(), OpenModes[FILE_READWRITE_TRUNCATE]);
		}

		if (!handle) {
			return false;
		}

		name = fileName;
		mode = fileMode;
		position = 0;
		readSyncNeeded = false;
		writeSyncNeeded = false;

		fseek((FILE*)handle, 0, SEEK_END);
		size = ftell((FILE*)handle);
		fseek((FILE*)handle, 0, SEEK_SET);
		return true;
	}

	size_t FileStream::Read(void* dest, size_t numBytes)
	{
		if (!handle) {
			return 0;
		}

		if (numBytes + position > size) {
			numBytes = size - position;
		}
		if (!numBytes) {
			return 0;
		}

		// Need to reassign the position due to internal buffering when transitioning from writing to reading
		if (readSyncNeeded) {
			fseek((FILE*)handle, (long)position, SEEK_SET);
			readSyncNeeded = false;
		}

		size_t ret = fread(dest, numBytes, 1, (FILE*)handle);
		if (ret != 1) {
			// If error, return to the position where the read began
			fseek((FILE*)handle, (long)position, SEEK_SET);
			return 0;
		}

		writeSyncNeeded = true;
		position += numBytes;
		return numBytes;
	}

	size_t FileStream::Write(const void* data, size_t numBytes)
	{
		if (!handle || mode == FILE_READ) {
			return 0;
		}

		if (!numBytes) {
			return 0;
		}

		// Need to reassign the position due to internal buffering when transitioning from reading to writing
		if (writeSyncNeeded) {
			fseek((FILE*)handle, (long)position, SEEK_SET);
			writeSyncNeeded = false;
		}

		if (fwrite(data, numBytes, 1, (FILE*)handle) != 1) {
			// If error, return to the position where the write began
			fseek((FILE*)handle, (long)position, SEEK_SET);
			return 0;
		}

		readSyncNeeded = true;
		position += numBytes;
		if (position > size) {
			size = position;
		}

		return numBytes;
	}

	size_t FileStream::Seek(size_t newPosition)
	{
		if (!handle) {
			return 0;
		}

		// Allow sparse seeks if writing
		if (mode == FILE_READ && newPosition > size) {
			newPosition = size;
		}

		fseek((FILE*)handle, (long)newPosition, SEEK_SET);
		position = newPosition;
		readSyncNeeded = false;
		writeSyncNeeded = false;
		return position;
	}

	bool FileStream::IsReadable() const
	{
		return handle != 0;
	}

	bool FileStream::IsWritable() const
	{
		return handle != 0 && mode != FILE_READ;
	}

	void FileStream::Close()
	{
		if (handle) {
			fclose((FILE*)handle);
			handle = 0;
			position = 0;
			size = 0;
		}
	}

	void FileStream::Flush()
	{
		if (handle) {
			fflush((FILE*)handle);
		}
	}

	bool FileStream::IsOpen() const
	{
		return handle != 0;
	}
}
