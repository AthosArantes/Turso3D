#pragma once

#include <cstdlib>
#include <cmath>
#include <limits>
#include <algorithm>

#undef M_PI

namespace Turso3D
{
	constexpr float M_PI = 3.14159265358979323846264338327950288f;
	constexpr float M_HALF_PI = M_PI * 0.5f;

	constexpr int M_MAX_INT = 0x7fffffff;

	constexpr float M_EPSILON = 0.000001f;
	constexpr float M_MAX_FLOAT = std::numeric_limits<float>::max();
	constexpr float M_INFINITY = std::numeric_limits<float>::infinity();
	constexpr float M_DEGTORAD = M_PI / 180.0f;
	constexpr float M_DEGTORAD_2 = M_PI / 360.0f;
	constexpr float M_RADTODEG = 1.0f / M_DEGTORAD;

	// Intersection test result.
	enum Intersection
	{
		OUTSIDE = 0,
		INTERSECTS,
		INSIDE
	};

	// Check whether two floating point values are equal within accuracy.
	inline bool EpsilonEquals(float lhs, float rhs, float epsilon = M_EPSILON)
	{
		return lhs + epsilon >= rhs && lhs - epsilon <= rhs;
	}

	// Linear interpolation between two float values.
	inline float Lerp(float lhs, float rhs, float t)
	{
		return lhs * (1.0f - t) + rhs * t;
	}

	// Clamp a float to a range.
	inline float Clamp(float value, float min, float max)
	{
		if (value < min) {
			return min;
		} else if (value > max) {
			return max;
		} else {
			return value;
		}
	}
	// Clamp an integer to a range.
	inline int Clamp(int value, int min, int max)
	{
		if (value < min) {
			return min;
		} else if (value > max) {
			return max;
		} else {
			return value;
		}
	}

	// Check whether an unsigned integer is a power of two.
	inline bool IsPowerOfTwo(unsigned value)
	{
		if (!value) {
			return true;
		}
		while (!(value & 1)) {
			value >>= 1;
		}
		return value == 1;
	}

	// Round up to next power of two.
	inline unsigned NextPowerOfTwo(unsigned value)
	{
		unsigned ret = 1;
		while (ret < value && ret < 0x80000000) {
			ret <<= 1;
		}
		return ret;
	}
}
