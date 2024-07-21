#include <Turso3D/Renderer/Batch.h>
#include <Turso3D/Renderer/GeometryNode.h>
#include <Turso3D/Renderer/Material.h>
#include <algorithm>

namespace Turso3D
{
	inline bool CompareBatchKeys(const Batch& lhs, const Batch& rhs)
	{
		return lhs.sortKey < rhs.sortKey;
	}

	inline bool CompareBatchDistance(const Batch& lhs, const Batch& rhs)
	{
		return lhs.distance > rhs.distance;
	}

	// ==========================================================================================
	void BatchQueue::Clear()
	{
		batches.clear();
	}

	void BatchQueue::Sort(std::vector<Matrix3x4>& instanceTransforms, BatchSortMode sortMode, bool convertToInstanced)
	{
		switch (sortMode) {
			case BatchSortMode::State:
				for (size_t i = 0; i < batches.size() - 1; ++i) {
					Batch& batch = batches[i];
					unsigned materialId = (unsigned)((size_t)batch.pass / sizeof(Pass));
					unsigned geomId = (unsigned)((size_t)batch.geometry / sizeof(Geometry));
					batch.sortKey = (((uint64_t)materialId) << 32) | geomId ^ batch.lightMask;
				}
				std::sort(batches.begin(), batches.end(), CompareBatchKeys);
				break;

			case BatchSortMode::StateDistance:
				for (size_t i = 0; i < batches.size() - 1; ++i) {
					Batch& batch = batches[i];
					unsigned materialId = batch.pass->lastSortKey.second;
					unsigned geomId = batch.geometry->lastSortKey.second;
					batch.sortKey = (((uint64_t)materialId) << 32) | geomId ^ batch.lightMask;
				}
				std::sort(batches.begin(), batches.end(), CompareBatchKeys);
				break;

			case BatchSortMode::Distance:
				std::sort(batches.begin(), batches.end(), CompareBatchDistance);
				break;

		}

		if (!convertToInstanced || batches.size() < 2) {
			return;
		}

		for (size_t i = 0; i < batches.size() - 1; ++i) {
			Batch& batch = batches[i];

			// Check if batch is static geometry and can be converted to instanced
			if (batch.type != BatchType::Static) {
				continue;
			}

			size_t instanceStart = instanceTransforms.size();
			size_t instanceCount = 0;
			for (size_t j = i + 1; j < batches.size(); ++j) {
				const Batch& next = batches[j];
				if (next.pass != batch.pass ||
					next.geometry != batch.geometry ||
					next.lightMask != batch.lightMask ||
					next.type != BatchType::Static
				) {
					break;
				}

				if (instanceCount == 0) {
					instanceTransforms.push_back(*batch.worldTransform);
					instanceCount = 1u;
				}

				instanceTransforms.push_back(*next.worldTransform);
				++instanceCount;
			}

			// Finalize the conversion by changing type and writing offsets.
			if (instanceCount) {
				batch.type = BatchType::Instanced;
				batch.instanceStart = instanceStart;
				batch.instanceCount = instanceCount;
				i += instanceCount - 1;
			}
		}
	}
}
