#pragma once

#include <cstddef>
#include <string>

namespace Turso3D
{
#ifdef _M_X64
	constexpr size_t FNV1a_InitHash = 0xCBF29CE484222325;
	constexpr size_t FNV1a_Prime = 0x100000001B3;
#else
	constexpr size_t FNV1a_InitHash = 0x811C9DC5;
	constexpr size_t FNV1a_Prime = 0x1000193;
#endif

	constexpr size_t FNV1aHash(const char* const str, const size_t _h = FNV1a_InitHash) noexcept
	{
		return (str[0] == '\0') ? _h : FNV1aHash(&str[1], (_h ^ size_t(str[0])) * FNV1a_Prime);
	}

	// ==========================================================================================
	struct StringHash
	{
		constexpr StringHash() noexcept : value(0)
		{
		}
		constexpr StringHash(const StringHash& hash) noexcept : value(hash.value)
		{
		}
		constexpr StringHash(const char* str) noexcept : value(FNV1aHash(str))
		{
		}
		StringHash(const std::string& str) noexcept
		{
			value = FNV1a_InitHash;
			for (const std::string::value_type& c : str) {
				value = (value ^ c) * FNV1a_Prime;
			}
		}

		constexpr StringHash& operator=(const StringHash& rhs) noexcept
		{
			value = rhs.value;
			return *this;
		}
		StringHash& operator=(const std::string& str) noexcept
		{
			value = StringHash {str}.value;
			return *this;
		}

		constexpr operator size_t() const noexcept
		{
			return value;
		}

		size_t value;
	};
}

template <>
struct std::hash<Turso3D::StringHash>
{
	size_t operator()(const Turso3D::StringHash& data) const noexcept
	{
		return data.value;
	}
};
