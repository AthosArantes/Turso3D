#pragma once

#include <cstddef>
#include <string>

namespace Turso3D
{
	struct StringHash
	{
		constexpr StringHash() noexcept : value(0)
		{
		}
		StringHash(const std::string& str) noexcept :
			value(std::hash<std::string>{}(str))
		{
		}

		StringHash& operator=(const StringHash&) noexcept = default;
		StringHash& operator=(const std::string& str) noexcept
		{
			value = std::hash<std::string> {}(str);
			return *this;
		}

		operator size_t() const noexcept
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
