#include "IntBox.h"
#include <fmt/format.h>
#include <cstdio>
#include <cstdlib>

namespace Turso3D
{
	const IntBox IntBox::ZERO(0, 0, 0, 0, 0, 0);

	bool IntBox::FromString(const char* string)
	{
		/*size_t elements = CountElements(string);
		if (elements < 6) {
			return false;
		}*/

		char* ptr = const_cast<char*>(string);
		left = strtol(ptr, &ptr, 10);
		top = strtol(ptr, &ptr, 10);
		near = strtol(ptr, &ptr, 10);
		right = strtol(ptr, &ptr, 10);
		bottom = strtol(ptr, &ptr, 10);
		far = strtol(ptr, &ptr, 10);

		return true;
	}

	std::string IntBox::ToString() const
	{
		return fmt::format("{} {} {} {} {} {}", left, top, near, right, bottom, far);
	}
}
