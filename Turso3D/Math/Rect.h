#pragma once

#include <Turso3D/Math/Math.h>
#include <Turso3D/Math/Vector4.h>
#include <utility>

namespace Turso3D
{
	// Two-dimensional bounding rectangle.
	class Rect
	{
	public:
		// Construct as undefined (negative size.)
		Rect() :
			min(Vector2 {M_INFINITY, M_INFINITY}),
			max(Vector2 {-M_INFINITY, -M_INFINITY})
		{
		}

		// Copy-construct.
		Rect(const Rect& rect) :
			min(rect.min),
			max(rect.max)
		{
		}

		// Construct from minimum and maximum vectors.
		Rect(const Vector2& min, const Vector2& max) :
			min(min),
			max(max)
		{
		}

		// Construct from coordinates.
		Rect(float left, float top, float right, float bottom) :
			min(left, top),
			max(right, bottom)
		{
		}

		// Construct from a Vector4.
		Rect(const Vector4& vector) :
			min(vector.x, vector.y),
			max(vector.z, vector.w)
		{
		}

		// Assign from another rect.
		Rect& operator = (const Rect& rhs)
		{
			min = rhs.min;
			max = rhs.max;
			return *this;
		}

		// Test for equality with another rect without epsilon.
		bool operator == (const Rect& rhs) const
		{
			return min == rhs.min && max == rhs.max;
		}
		// Test for inequality with another rect without epsilon.
		bool operator != (const Rect& rhs) const
		{
			return !(*this == rhs);
		}

		// Define from another rect.
		void Define(const Rect& rect)
		{
			min = rect.min;
			max = rect.max;
		}

		// Define from minimum and maximum vectors.
		void Define(const Vector2& min, const Vector2& max)
		{
			this->min = min;
			this->max = max;
		}

		// Define from a point.
		void Define(const Vector2& point)
		{
			min = max = point;
		}

		// Merge a point.
		void Merge(const Vector2& point)
		{
			// If undefined, set initial dimensions
			if (!IsDefined()) {
				min = max = point;
				return;
			}

			if (point.x < min.x) {
				min.x = point.x;
			}
			if (point.x > max.x) {
				max.x = point.x;
			}
			if (point.y < min.y) {
				min.y = point.y;
			}
			if (point.y > max.y) {
				max.y = point.y;
			}
		}

		// Merge a rect.
		void Merge(const Rect& rect)
		{
			if (min.x > max.x) {
				min = rect.min;
				max = rect.max;
				return;
			}

			if (rect.min.x < min.x) {
				min.x = rect.min.x;
			}
			if (rect.min.y < min.y) {
				min.y = rect.min.y;
			}
			if (rect.max.x > max.x) {
				max.x = rect.max.x;
			}
			if (rect.max.y > max.y) {
				max.y = rect.max.y;
			}
		}

		// Set as undefined to allow the next merge to set initial size.
		void Undefine()
		{
			min = Vector2 {M_INFINITY, M_INFINITY};
			max = -min;
		}

		// Clip with another rect.
		void Clip(const Rect& rect)
		{
			if (rect.min.x > min.x) {
				min.x = rect.min.x;
			}
			if (rect.max.x < max.x) {
				max.x = rect.max.x;
			}
			if (rect.min.y > min.y) {
				min.y = rect.min.y;
			}
			if (rect.max.y < max.y) {
				max.y = rect.max.y;
			}

			if (min.x > max.x) {
				std::swap(min.x, max.x);
			}
			if (min.y > max.y) {
				std::swap(min.y, max.y);
			}
		}

		// Return whether has non-negative size.
		bool IsDefined() const
		{
			return (min.x <= max.x);
		}

		// Return center.
		Vector2 Center() const
		{
			return (max + min) * 0.5f;
		}

		// Return size.
		Vector2 Size() const
		{
			return max - min;
		}

		// Return half-size.
		Vector2 HalfSize() const
		{
			return (max - min) * 0.5f;
		}

		// Test for equality with another rect with epsilon.
		bool Equals(const Rect& rhs, float epsilon = M_EPSILON) const
		{
			return min.Equals(rhs.min, epsilon) && max.Equals(rhs.max, epsilon);
		}

		// Test whether a point is inside.
		Intersection IsInside(const Vector2& point) const
		{
			if (point.x < min.x || point.y < min.y || point.x > max.x || point.y > max.y) {
				return OUTSIDE;
			}
			return INSIDE;
		}

		// Return as a vector.
		Vector4 ToVector4() const
		{
			return Vector4(min.x, min.y, max.x, max.y);
		}

		// Rect in the range (-1, -1) - (1, 1)
		static Rect FULL();
		// Rect in the range (0, 0) - (1, 1)
		static Rect POSITIVE();
		// Zero-sized rect.
		static Rect ZERO();

	public:
		// Minimum vector.
		Vector2 min;
		// Maximum vector.
		Vector2 max;
	};

	// ==========================================================================================
	inline Rect Rect::FULL()
	{
		return Rect {-1.0f, -1.0f, 1.0f, 1.0f};
	}

	inline Rect Rect::POSITIVE()
	{
		return Rect {0.0f, 0.0f, 1.0f, 1.0f};
	}

	inline Rect Rect::ZERO()
	{
		return Rect {0.0f, 0.0f, 0.0f, 0.0f};
	}
}
