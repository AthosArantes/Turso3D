#pragma once

#include <Turso3D/Math/Math.h>

namespace Turso3D
{
	// Two-dimensional vector with integer values.
	class IntVector2
	{
	public:
		// Construct undefined.
		IntVector2()
		{
		}

		// Copy-construct.
		IntVector2(const IntVector2& vector) :
			x(vector.x),
			y(vector.y)
		{
		}

		// Construct from coordinates.
		IntVector2(int x, int y) :
			x(x),
			y(y)
		{
		}

		// Construct from an int array.
		IntVector2(const int* data) :
			x(data[0]),
			y(data[1])
		{
		}

		// Add-assign a vector.
		IntVector2& operator += (const IntVector2& rhs)
		{
			x += rhs.x;
			y += rhs.y;
			return *this;
		}

		// Subtract-assign a vector.
		IntVector2& operator -= (const IntVector2& rhs)
		{
			x -= rhs.x;
			y -= rhs.y;
			return *this;
		}

		// Multiply-assign a scalar.
		IntVector2& operator *= (int rhs)
		{
			x *= rhs;
			y *= rhs;
			return *this;
		}

		// Divide-assign a scalar.
		IntVector2& operator /= (int rhs)
		{
			x /= rhs;
			y /= rhs;
			return *this;
		}

		// Test for equality with another vector.
		bool operator == (const IntVector2& rhs) const { return x == rhs.x && y == rhs.y; }
		// Test for inequality with another vector.
		bool operator != (const IntVector2& rhs) const { return !(*this == rhs); }
		// Add a vector.
		IntVector2 operator + (const IntVector2& rhs) const { return IntVector2(x + rhs.x, y + rhs.y); }
		// Return negation.
		IntVector2 operator - () const { return IntVector2(-x, -y); }
		// Subtract a vector.
		IntVector2 operator - (const IntVector2& rhs) const { return IntVector2(x - rhs.x, y - rhs.y); }
		// Multiply with a scalar.
		IntVector2 operator * (int rhs) const { return IntVector2(x * rhs, y * rhs); }
		// Divide by a scalar.
		IntVector2 operator / (int rhs) const { return IntVector2(x / rhs, y / rhs); }

		// Return integer data.
		const int* Data() const { return &x; }

		// Zero vector.
		static IntVector2 ZERO();

	public:
		// X coordinate.
		int x;
		// Y coordinate.
		int y;
	};

	// ==========================================================================================
	// Multiply IntVector2 with a scalar.
	inline IntVector2 operator * (int lhs, const IntVector2& rhs)
	{
		return rhs * lhs;
	}

	inline IntVector2 IntVector2::ZERO() { return IntVector2 {0, 0}; }
}
