#include "IntVector2.h"
#include <fmt/format.h>
#include <cstdio>
#include <cstdlib>

namespace Turso3D
{
	const IntVector2 IntVector2::ZERO(0, 0);

	bool IntVector2::FromString(const char* string)
	{
		/*size_t elements = CountElements(string);
		if (elements < 2) {
			return false;
		}*/

		char* ptr = const_cast<char*>(string);
		x = strtol(ptr, &ptr, 10);
		y = strtol(ptr, &ptr, 10);

		return true;
	}

	std::string IntVector2::ToString() const
	{
		return fmt::format("{} {}", x, y);
	}
}
