#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <string>
#include <vector>
#include <type_traits>

namespace Turso3D
{
	// Abstract stream for reading and writing.
	class Stream
	{
	public:
		// Default-construct with zero size.
		Stream();
		// Destruct.
		virtual ~Stream();

		// Read bytes from the stream.
		// Return number of bytes actually read.
		virtual size_t Read(void* dest, size_t numBytes) = 0;
		// Write bytes to the stream.
		// Return number of bytes actually written.
		virtual size_t Write(const void* data, size_t size) = 0;
		// Set position in bytes from the beginning of the stream.
		// Return the position after the seek.
		virtual size_t Seek(size_t position) = 0;

		// Return whether read operations are allowed.
		virtual bool IsReadable() const = 0;
		// Return whether write operations are allowed.
		virtual bool IsWritable() const = 0;

		// Change the stream name.
		void SetName(const std::string& newName);
		// Change the stream name.
		void SetName(const char* newName);
		// Return the stream name.
		const std::string& Name() const { return name; }

		// Read a string line.
		std::string ReadLine();
		// Write a string and append a line break.
		void WriteLine(const std::string& data);

		// Return current position in bytes.
		size_t Position() const { return position; }
		// Return size in bytes.
		size_t Size() const { return size; }
		// Return whether the end of stream has been reached.
		bool IsEof() const { return position >= size; }

		// ==========================================================================================
		// Write operators

		template <typename T>
		size_t Write(const T& data)
		{
			size_t w = Write(&data, sizeof(T));
			assert(w == sizeof(T) && "Amount of bytes written differs from size of T");
			return w;
		}

		template <typename T>
		size_t Write(const std::vector<T>& data)
		{
			size_t w = Write(data.size());
			for (const T& x : data)
			{
				w += Write<T>(x);
			}
			return w;
		}

		template <>
		size_t Write<bool>(const bool& data)
		{
			return Write(data ? (uint8_t)1u : (uint8_t)0u);
		}

		// Write a null terminated string.
		template <>
		size_t Write<std::string>(const std::string& data)
		{
			size_t w = Write(data.c_str(), data.size() + 1);
			assert(w == (data.size() + 1) && "String length mismatch");
			return w;
		}

		// ==========================================================================================
		// Read operators

		template <typename T>
		size_t Read(T& output)
		{
			size_t r = Read(&output, sizeof(T));
			assert(r == sizeof(T) && "Amount of bytes read differs from size of T");
			return r;
		}

		template <typename T>
		size_t Read(std::vector<T>& output)
		{
			size_t count;
			size_t r = Read<size_t>(count);

			if (output.capacity() - output.size() < count) {
				output.reserve(output.size() + count);
			}

			for (size_t i = 0; i < count; ++i)
			{
				T& data = output.emplace_back();
				r += Read<T>(data);
			}

			return r;
		}

		template <typename T>
		auto Read() -> std::enable_if_t<std::is_default_constructible_v<T>, std::remove_cv_t<T>>
		{
			std::remove_cv_t<T> value;
			Read<T>(value);
			return value;
		}

		template <>
		size_t Read<bool>(bool& output)
		{
			uint8_t value;
			size_t r = Read<uint8_t>(value);
			output = (value != 0) ? true : false;
			return r;
		}

		// Read a null terminated string.
		template <>
		size_t Read<std::string>(std::string& output)
		{
			size_t r = 0;
			while (!IsEof()) {
				char c;
				r += Read<char>(c);
				if (!c) {
					break;
				}
				output += c;
			}
			return r;
		}

	protected:
		// Stream position.
		size_t position;
		// Stream size.
		size_t size;
		// Stream name.
		std::string name;
	};
}
