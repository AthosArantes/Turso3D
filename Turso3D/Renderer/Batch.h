#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace Turso3D
{
	class GeometryDrawable;
	class Pass;
	class Matrix3x4;
	struct Geometry;

	// Sorting modes for batches.
	enum BatchSortMode
	{
		BATCH_SORT_STATE,
		BATCH_SORT_STATE_DISTANCE,
		BATCH_SORT_DISTANCE
	};

	enum BatchType
	{
		// Simple static geometry rendering, the batch contains a worldTransform.
		BATCH_TYPE_STATIC,
		// Complex geometry rendering, the batch contains a drawable.
		BATCH_TYPE_COMPLEX,
		// The batch was converted from Static to instance, the batch contains instance count.
		BATCH_TYPE_INSTANCED
	};

	// Stored draw call.
	struct Batch
	{
		union
		{
			// State sorting key.
			uint64_t sortKey;
			// Distance for alpha batches.
			float distance;
			// Start position in the instance vertex buffer if instanced.
			size_t instanceStart;
		};

		// Material pass.
		Pass* pass;
		// Geometry.
		Geometry* geometry;
		// Geometry index.
		unsigned geomIndex;

		// The content type of this batch.
		BatchType type;
		// Drawable flags.
		unsigned drawableFlags;
		// Drawable light contribution mask.
		unsigned lightMask;

		union
		{
			// Pointer to world transform matrix for static geometry rendering.
			const Matrix3x4* worldTransform;
			// Associated drawable.
			// Called into for complex rendering like skinning.
			GeometryDrawable* drawable;
			// Instance count if instanced.
			size_t instanceCount;
		};
	};

	// Collection of draw calls with sorting and instancing functionality.
	struct BatchQueue
	{
		// Clear for the next frame.
		void Clear();
		// Sort batches and setup instancing groups.
		void Sort(std::vector<Matrix3x4>& instanceTransforms, BatchSortMode sortMode, bool convertToInstanced);
		// Return whether has batches added.
		bool HasBatches() const { return batches.size(); }

		// Batches.
		std::vector<Batch> batches;
	};
}
