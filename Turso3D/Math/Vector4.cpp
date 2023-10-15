#include "Vector4.h"
#include <fmt/format.h>
#include <cstdio>
#include <cstdlib>

namespace Turso3D
{
	const Vector4 Vector4::ZERO(0.0f, 0.0f, 0.0f, 0.0f);
	const Vector4 Vector4::ONE(1.0f, 1.0f, 1.0f, 1.0f);

	bool Vector4::FromString(const char* string)
	{
		/*size_t elements = CountElements(string);
		if (elements < 4) {
			return false;
		}*/

		char* ptr = const_cast<char*>(string);
		x = (float)strtod(ptr, &ptr);
		y = (float)strtod(ptr, &ptr);
		z = (float)strtod(ptr, &ptr);
		w = (float)strtod(ptr, &ptr);

		return true;
	}

	std::string Vector4::ToString() const
	{
		return fmt::format("{} {} {} {}", x, y, z, w);
	}
}
