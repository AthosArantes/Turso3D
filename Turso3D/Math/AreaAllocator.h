#pragma once

#include <Turso3D/Math/IntRect.h>
#include <vector>

namespace Turso3D
{
	// AreaAllocator code inspired by https://github.com/juj/RectangleBinPack

	// Rectangular area allocator.
	class AreaAllocator
	{
	public:
		// Default construct with empty size.
		AreaAllocator()
		{
			Reset(0, 0);
		}

		// Construct with given width and height.
		AreaAllocator(int width, int height, bool fastMode = true)
		{
			Reset(width, height, fastMode);
		}

		// Construct with given width and height, and set the maximum allowed to grow to.
		AreaAllocator(int width, int height, int maxWidth, int maxHeight, bool fastMode = true)
		{
			Reset(width, height, maxWidth, maxHeight, fastMode);
		}

		// Reset to given width and height and remove all previous allocations.
		void Reset(int width, int height, int maxWidth = 0, int maxHeight = 0, bool fastMode = true)
		{
			doubleWidth = true;
			size = IntVector2 {width, height};
			maxSize = IntVector2 {maxWidth, maxHeight};
			this->fastMode = fastMode;

			freeAreas.clear();
			IntRect initialArea(0, 0, width, height);
			freeAreas.push_back(initialArea);
		}

		// Try to allocate a rectangle.
		// Return true on success, with x & y coordinates filled.
		bool Allocate(int width, int height, int& x, int& y)
		{
			if (width < 0) {
				width = 0;
			}
			if (height < 0) {
				height = 0;
			}

			std::vector<IntRect>::iterator best;
			int bestFreeArea;

			for (;;) {
				best = freeAreas.end();
				bestFreeArea = M_MAX_INT;
				for (auto i = freeAreas.begin(); i != freeAreas.end(); ++i) {
					int freeWidth = i->Width();
					int freeHeight = i->Height();

					if (freeWidth >= width && freeHeight >= height) {
						// Calculate rank for free area. Lower is better
						int freeArea = freeWidth * freeHeight;

						if (freeArea < bestFreeArea) {
							best = i;
							bestFreeArea = freeArea;
						}
					}
				}

				if (best == freeAreas.end()) {
					if (doubleWidth && size.x < maxSize.x) {
						int oldWidth = size.x;
						size.x <<= 1;
						// If no allocations yet, simply expand the single free area
						IntRect& first = freeAreas.front();
						if (freeAreas.size() == 1 && first.left == 0 && first.top == 0 && first.right == oldWidth && first.bottom == size.y) {
							first.right = size.x;
						} else {
							IntRect newArea(oldWidth, 0, size.x, size.y);
							freeAreas.push_back(newArea);
						}
					} else if (!doubleWidth && size.y < maxSize.y) {
						int oldHeight = size.y;
						size.y <<= 1;
						IntRect& first = freeAreas.front();
						if (freeAreas.size() == 1 && first.left == 0 && first.top == 0 && first.right == size.x && first.bottom == oldHeight) {
							first.bottom = size.y;
						} else {
							IntRect newArea(0, oldHeight, size.x, size.y);
							freeAreas.push_back(newArea);
						}
					} else {
						return false;
					}
					doubleWidth = !doubleWidth;
				} else {
					break;
				}
			}

			IntRect reserved(best->left, best->top, best->left + width, best->top + height);
			x = best->left;
			y = best->top;

			if (fastMode) {
				// Reserve the area by splitting up the remaining free area
				best->left = reserved.right;
				if (best->Height() > 2 * height || height >= size.y / 2) {
					IntRect splitArea(reserved.left, reserved.bottom, best->right, best->bottom);
					best->bottom = reserved.bottom;
					freeAreas.push_back(splitArea);
				}
			} else {
				// Remove the reserved area from all free areas
				for (size_t i = 0; i < freeAreas.size();) {
					if (SplitRect(freeAreas[i], reserved)) {
						freeAreas.erase(freeAreas.begin() + i);
					} else {
						++i;
					}
				}
				Cleanup();
			}

			return true;
		}

		// Attempt a specific allocation.
		// Return true on success.
		bool AllocateSpecific(const IntRect& reserved)
		{
			bool success = false;

			for (size_t i = 0; i < freeAreas.size(); ++i) {
				if (freeAreas[i].IsInside(reserved) == INSIDE) {
					success = true;
					break;
				}
			}

			if (!success) {
				return false;
			}

			// Remove the reserved area from all free areas
			for (size_t i = 0; i < freeAreas.size();) {
				if (SplitRect(freeAreas[i], reserved)) {
					freeAreas.erase(freeAreas.begin() + i);
				} else {
					++i;
				}
			}

			Cleanup();
			return true;
		}

		// Return the current size.
		const IntVector2& Size() const { return size; }
		// Return the current width.
		int Width() const { return size.x; }
		// Return the current height.
		int Height() const { return size.y; }

		// Return the maximum size.
		const IntVector2& MaxSize() const { return maxSize; }
		// Return the maximum width.
		int MaxWidth() const { return maxSize.x; }
		// Return the maximum height.
		int MaxHeight() const { return maxSize.y; }

		// Return whether uses fast mode.
		// Fast mode uses a simpler allocation scheme which may waste free space, but is OK for eg. fonts.
		bool IsFastMode() const { return fastMode; }

	private:
		// Remove space from a free rectangle.
		// Return true if the original rectangle should be erased from the free list.
		// Not called in fast mode.
		bool SplitRect(IntRect original, const IntRect& reserve)
		{
			if (reserve.right > original.left && reserve.left < original.right && reserve.bottom > original.top && reserve.top < original.bottom) {
				// Check for splitting from the right
				if (reserve.right < original.right) {
					IntRect newRect = original;
					newRect.left = reserve.right;
					freeAreas.push_back(newRect);
				}
				// Check for splitting from the left
				if (reserve.left > original.left) {
					IntRect newRect = original;
					newRect.right = reserve.left;
					freeAreas.push_back(newRect);
				}
				// Check for splitting from the bottom
				if (reserve.bottom < original.bottom) {
					IntRect newRect = original;
					newRect.top = reserve.bottom;
					freeAreas.push_back(newRect);
				}
				// Check for splitting from the top
				if (reserve.top > original.top) {
					IntRect newRect = original;
					newRect.bottom = reserve.top;
					freeAreas.push_back(newRect);
				}

				return true;
			}

			return false;
		}

		// Clean up redundant free space.
		// Not called in fast mode.
		void Cleanup()
		{
			// Remove rects which are contained within another rect
			for (size_t i = 0; i < freeAreas.size();) {
				bool erased = false;
				for (size_t j = i + 1; j < freeAreas.size();) {
					if ((freeAreas[i].left >= freeAreas[j].left) &&
						(freeAreas[i].top >= freeAreas[j].top) &&
						(freeAreas[i].right <= freeAreas[j].right) &&
						(freeAreas[i].bottom <= freeAreas[j].bottom)) {
						freeAreas.erase(freeAreas.begin() + i);
						erased = true;
						break;
					}
					if ((freeAreas[j].left >= freeAreas[i].left) &&
						(freeAreas[j].top >= freeAreas[i].top) &&
						(freeAreas[j].right <= freeAreas[i].right) &&
						(freeAreas[j].bottom <= freeAreas[i].bottom)) {
						freeAreas.erase(freeAreas.begin() + j);
					} else {
						++j;
					}
				}

				if (!erased) {
					++i;
				}
			}
		}

	private:
		// Free rectangles.
		std::vector<IntRect> freeAreas;

		// Current size.
		IntVector2 size;
		// Maximum size allowed to grow to.
		// It is zero when it is not allowed to grow.
		IntVector2 maxSize;

		// The dimension used for next growth. Used internally.
		bool doubleWidth;
		// Fast mode flag.
		bool fastMode;
	};
}
