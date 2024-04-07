#pragma once

#include <Turso3D/Math/IntVector3.h>

namespace Turso3D
{
	// Three-dimensional bounding box with integer values.
	class IntBox
	{
	public:
		// Construct undefined.
		IntBox()
		{
		}

		// Copy-construct.
		IntBox(const IntBox& box) :
			left(box.left),
			top(box.top),
			near(box.near),
			right(box.right),
			bottom(box.bottom),
			far(box.far)
		{
		}

		// Construct from coordinates.
		IntBox(int left, int top, int near, int right, int bottom, int far) :
			left(left),
			top(top),
			near(near),
			right(right),
			bottom(bottom),
			far(far)
		{
		}

		// Test for equality with another box.
		bool operator == (const IntBox& rhs) const
		{
			return left == rhs.left && top == rhs.top && right == rhs.right && bottom == rhs.bottom && near == rhs.near && far == rhs.far;
		}
		// Test for inequality with another box.
		bool operator != (const IntBox& rhs) const
		{
			return !(*this == rhs);
		}

		// Return size.
		IntVector3 Size() const { return IntVector3 {Width(), Height(), Depth()}; }
		// Return width.
		int Width() const { return right - left; }
		// Return height.
		int Height() const { return bottom - top; }
		// Return depth.
		int Depth() const { return far - near; }

		void SetSize(int width, int height, int depth)
		{
			right = left + width;
			bottom = top + height;
			far = near + depth;
		}

		// Test whether a point is inside.
		Intersection IsInside(const IntVector3& point) const
		{
			if (point.x < left || point.y < top || point.z < near || point.x >= right || point.y >= bottom || point.z >= far) {
				return OUTSIDE;
			}
			return INSIDE;
		}

		// Test whether another box is inside or intersects.
		Intersection IsInside(const IntBox& box) const
		{
			if (box.right <= left || box.left >= right || box.bottom <= top || box.top >= bottom || box.far <= near || box.near >= far) {
				return OUTSIDE;
			} else if (box.left >= left && box.right <= right && box.top >= top && box.bottom <= bottom && box.near >= near && box.far <= far) {
				return INSIDE;
			}
			return INTERSECTS;
		}

		// Zero-sized box.
		static IntBox ZERO();

	public:
		// Left coordinate.
		int left;
		// Top coordinate.
		int top;
		// Near coordinate.
		int near;
		// Right coordinate.
		int right;
		// Bottom coordinate.
		int bottom;
		// Far coordinate.
		int far;
	};

	// ==========================================================================================
	inline IntBox IntBox::ZERO()
	{
		return IntBox {0, 0, 0, 0, 0, 0};
	}
}
