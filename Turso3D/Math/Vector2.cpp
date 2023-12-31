#include "Vector2.h"
#include <fmt/format.h>
#include <cstdio>
#include <cstdlib>

namespace Turso3D
{
	const Vector2 Vector2::ZERO(0.0f, 0.0f);
	const Vector2 Vector2::LEFT(-1.0f, 0.0f);
	const Vector2 Vector2::RIGHT(1.0f, 0.0f);
	const Vector2 Vector2::UP(0.0f, 1.0f);
	const Vector2 Vector2::DOWN(0.0f, -1.0f);
	const Vector2 Vector2::ONE(1.0f, 1.0f);

	bool Vector2::FromString(const char* string)
	{
		/*size_t elements = CountElements(string);
		if (elements < 2) {
			return false;
		}*/

		char* ptr = const_cast<char*>(string);
		x = (float)strtod(ptr, &ptr);
		y = (float)strtod(ptr, &ptr);

		return true;
	}

	std::string Vector2::ToString() const
	{
		return fmt::format("{} {}", x, y);
	}
}
