#pragma once

#include <Turso3D/Math/Math.h>

namespace Turso3D
{
	// Three-dimensional vector with integer values.
	class IntVector3
	{
	public:
		// Construct undefined.
		IntVector3()
		{
		}

		// Copy-construct.
		IntVector3(const IntVector3& vector) :
			x(vector.x),
			y(vector.y),
			z(vector.z)
		{
		}

		// Construct from coordinates.
		IntVector3(int x, int y, int z) :
			x(x),
			y(y),
			z(z)
		{
		}

		// Construct from an int array.
		IntVector3(const int* data) :
			x(data[0]),
			y(data[1]),
			z(data[2])
		{
		}

		// Add-assign a vector.
		IntVector3& operator += (const IntVector3& rhs)
		{
			x += rhs.x;
			y += rhs.y;
			z += rhs.z;
			return *this;
		}

		// Subtract-assign a vector.
		IntVector3& operator -= (const IntVector3& rhs)
		{
			x -= rhs.x;
			y -= rhs.y;
			z -= rhs.z;
			return *this;
		}

		// Multiply-assign a scalar.
		IntVector3& operator *= (int rhs)
		{
			x *= rhs;
			y *= rhs;
			z *= rhs;
			return *this;
		}

		// Divide-assign a scalar.
		IntVector3& operator /= (int rhs)
		{
			x /= rhs;
			y /= rhs;
			z /= rhs;
			return *this;
		}

		// Test for equality with another vector.
		bool operator == (const IntVector3& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }
		// Test for inequality with another vector.
		bool operator != (const IntVector3& rhs) const { return !(*this == rhs); }
		// Add a vector.
		IntVector3 operator + (const IntVector3& rhs) const { return IntVector3(x + rhs.x, y + rhs.y, z + rhs.z); }
		// Return negation.
		IntVector3 operator - () const { return IntVector3(-x, -y, -z); }
		// Subtract a vector.
		IntVector3 operator - (const IntVector3& rhs) const { return IntVector3(x - rhs.x, y - rhs.y, z - rhs.z); }
		// Multiply with a scalar.
		IntVector3 operator * (int rhs) const { return IntVector3(x * rhs, y * rhs, z * rhs); }
		// Divide by a scalar.
		IntVector3 operator / (int rhs) const { return IntVector3(x / rhs, y / rhs, z / rhs); }

		// Return integer data.
		const int* Data() const { return &x; }

		// Zero vector.
		static IntVector3 ZERO();

	public:
		// X coordinate.
		int x;
		// Y coordinate.
		int y;
		// Z coordinate.
		int z;
	};

	// ==========================================================================================
	// Multiply IntVector2 with a scalar.
	inline IntVector3 operator * (int lhs, const IntVector3& rhs)
	{
		return rhs * lhs;
	}

	inline IntVector3 IntVector3::ZERO()
	{
		return IntVector3 {0, 0, 0};
	}
}
