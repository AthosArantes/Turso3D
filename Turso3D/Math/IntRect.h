#pragma once

#include <Turso3D/Math/Math.h>
#include <Turso3D/Math/IntVector2.h>
#include <Turso3D/Math/IntVector3.h>

namespace Turso3D
{
	// Two-dimensional bounding rectangle with integer values.
	class IntRect
	{
	public:
		// Construct undefined.
		IntRect()
		{
		}

		// Copy-construct.
		IntRect(const IntRect& rect) :
			left(rect.left),
			top(rect.top),
			right(rect.right),
			bottom(rect.bottom)
		{
		}

		// Construct from coordinates.
		IntRect(int left, int top, int right, int bottom) :
			left(left),
			top(top),
			right(right),
			bottom(bottom)
		{
		}

		// Construct from coordinates.
		IntRect(const IntVector2& leftTop, const IntVector2& rightBottom) :
			left(leftTop.x),
			top(leftTop.y),
			right(rightBottom.x),
			bottom(rightBottom.y)
		{
		}

		IntRect& operator = (const IntRect& rhs)
		{
			left = rhs.left;
			top = rhs.top;
			right = rhs.right;
			bottom = rhs.bottom;
			return *this;
		}

		// Test for equality with another rect.
		bool operator == (const IntRect& rhs) const
		{
			return !(left != rhs.left || top != rhs.top || right != rhs.right || bottom != rhs.bottom);
		}
		// Test for inequality with another rect.
		bool operator != (const IntRect& rhs) const
		{
			return !(*this == rhs);
		}

		// Return size
		IntVector2 Size() const { return IntVector2 {right - left, bottom - top}; }
		// Return width.
		int Width() const { return right - left; }
		// Return height.
		int Height() const { return bottom - top; }

		// Test whether a point is inside.
		Intersection IsInside(const IntVector2& point) const
		{
			if (point.x < left || point.y < top || point.x >= right || point.y >= bottom) {
				return OUTSIDE;
			}
			return INSIDE;
		}

		// Test whether another rect is inside or intersects.
		Intersection IsInside(const IntRect& rect) const
		{
			if (rect.right <= left || rect.left >= right || rect.bottom <= top || rect.top >= bottom) {
				return OUTSIDE;
			} else if (rect.left >= left && rect.right <= right && rect.top >= top && rect.bottom <= bottom) {
				return INSIDE;
			}
			return INTERSECTS;
		}

		static IntRect ZERO();

	public:
		int left;
		int top;
		int right;
		int bottom;
	};

	// ==========================================================================================
	inline IntRect IntRect::ZERO()
	{
		return IntRect {0, 0, 0, 0};
	}
}
