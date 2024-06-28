#include <Turso3D/Renderer/Octree.h>
#include <Turso3D/Core/WorkQueue.h>
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Math/Random.h>
#include <Turso3D/Math/Ray.h>
#include <Turso3D/Renderer/DebugRenderer.h>
#include <cassert>
#include <algorithm>

namespace
{
	using namespace Turso3D;

	constexpr float DEFAULT_OCTREE_SIZE = 1000.0f;
	constexpr int DEFAULT_OCTREE_LEVELS = 8;
	constexpr int MAX_OCTREE_LEVELS = 255;
	constexpr size_t MIN_THREADED_UPDATE = 16;

	static inline bool CompareRaycastResults(const RaycastResult& lhs, const RaycastResult& rhs)
	{
		return lhs.distance < rhs.distance;
	}

	static inline bool CompareDrawableDistances(const std::pair<Drawable*, float>& lhs, const std::pair<Drawable*, float>& rhs)
	{
		return lhs.second < rhs.second;
	}

	static inline bool CompareDrawables(Drawable* lhs, Drawable* rhs)
	{
		unsigned lhsFlags = lhs->Flags() & (Drawable::FLAG_LIGHT | Drawable::FLAG_GEOMETRY);
		unsigned rhsFlags = rhs->Flags() & (Drawable::FLAG_LIGHT | Drawable::FLAG_GEOMETRY);
		if (lhsFlags != rhsFlags) {
			return lhsFlags < rhsFlags;
		} else {
			return lhs < rhs;
		}
	}
}

namespace Turso3D
{
	// Task for octree drawables reinsertion.
	struct Octree::ReinsertDrawablesTask : public MemberFunctionTask<Octree>
	{
		// Construct.
		ReinsertDrawablesTask(Octree* object_, MemberWorkFunctionPtr function_) :
			MemberFunctionTask<Octree>(object_, function_)
		{
		}

		// Start pointer.
		Drawable** start;
		// End pointer.
		Drawable** end;
	};

	// ==========================================================================================
	Octant::Octant() :
		graphics(nullptr),
		parent(nullptr),
		visibility(VIS_VISIBLE_UNKNOWN),
		occlusionQueryId(0),
		occlusionQueryTimer(Random()* OCCLUSION_QUERY_INTERVAL),
		numChildren(0)
	{
		for (size_t i = 0; i < NUM_OCTANTS; ++i) {
			children[i] = nullptr;
		}
	}

	Octant::~Octant()
	{
		if (occlusionQueryId) {
			if (graphics) {
				graphics->FreeOcclusionQuery(occlusionQueryId);
			}
		}
	}

	void Octant::Initialize(Graphics* graphics_, Octant* parent_, const BoundingBox& boundingBox, unsigned char level_, unsigned char childIndex_)
	{
		BoundingBox worldBoundingBox = boundingBox;
		center = worldBoundingBox.Center();
		halfSize = worldBoundingBox.HalfSize();
		fittingBox = BoundingBox(worldBoundingBox.min - halfSize, worldBoundingBox.max + halfSize);

		graphics = graphics_;
		parent = parent_;
		level = level_;
		childIndex = childIndex_;
		flags = FLAG_CULLING_BOX_DIRTY;
	}

	void Octant::OnRenderDebug(DebugRenderer* debug)
	{
		debug->AddBoundingBox(CullingBox(), Color::GRAY(), true);
	}

	void Octant::OnOcclusionQuery(unsigned queryId)
	{
		// Should not have an existing query in flight
		assert(!occlusionQueryId);

		// Mark pending
		occlusionQueryId = queryId;
	}

	void Octant::OnOcclusionQueryResult(bool visible)
	{
		// Mark not pending
		occlusionQueryId = 0;

		// Do not change visibility if currently outside the frustum
		if (visibility == VIS_OUTSIDE_FRUSTUM) {
			return;
		}

		OctantVisibility lastVisibility = (OctantVisibility)visibility;
		OctantVisibility newVisibility = visible ? VIS_VISIBLE : VIS_OCCLUDED;

		visibility = newVisibility;

		if (lastVisibility <= VIS_OCCLUDED_UNKNOWN && newVisibility == VIS_VISIBLE) {
			// If came into view after being occluded, mark children as still occluded but that should be tested in hierarchy
			if (numChildren) {
				PushVisibilityToChildren(this, VIS_OCCLUDED_UNKNOWN);
			}
		} else if (newVisibility == VIS_OCCLUDED && lastVisibility != VIS_OCCLUDED && parent && parent->visibility == VIS_VISIBLE) {
			// If became occluded, mark parent unknown so it will be tested next
			parent->visibility = VIS_VISIBLE_UNKNOWN;
		}

		// Whenever is visible, push visibility to parents if they are not visible yet
		if (newVisibility == VIS_VISIBLE) {
			Octant* octant = parent;
			while (octant && octant->visibility != newVisibility) {
				octant->visibility = newVisibility;
				octant = octant->parent;
			}
		}
	}

	const BoundingBox& Octant::CullingBox() const
	{
		if (TestFlag(FLAG_CULLING_BOX_DIRTY)) {
			if (!numChildren && drawables.empty()) {
				cullingBox.Define(center);
			} else {
				// Use a temporary bounding box for calculations in case many threads call this simultaneously
				BoundingBox tempBox;

				for (size_t i = 0; i < drawables.size(); ++i) {
					tempBox.Merge(drawables[i]->WorldBoundingBox());
				}

				if (numChildren) {
					for (size_t i = 0; i < NUM_OCTANTS; ++i) {
						if (children[i]) {
							tempBox.Merge(children[i]->CullingBox());
						}
					}
				}

				cullingBox = tempBox;
			}

			SetFlag(FLAG_CULLING_BOX_DIRTY, false);
		}

		return cullingBox;
	}

	// ==========================================================================================
	Octree::Octree(WorkQueue* workQueue, Graphics* graphics) :
		threadedUpdate(false),
		workQueue(workQueue),
		graphics(graphics),
		frameNumber(0)
	{
		root.Initialize(graphics, nullptr, BoundingBox(-DEFAULT_OCTREE_SIZE, DEFAULT_OCTREE_SIZE), DEFAULT_OCTREE_LEVELS, 0);

		// Have at least 1 task for reinsert processing
		reinsertTasks.push_back(std::make_unique<ReinsertDrawablesTask>(this, &Octree::CheckReinsertWork));
		reinsertQueues = std::make_unique<std::vector<Drawable*>[]>(workQueue->NumThreads());
	}

	Octree::~Octree()
	{
		// Clear octree association from nodes that were never inserted
		// Note: the threaded queues cannot have nodes that were never inserted, only nodes that should be moved
		for (size_t i = 0; i < updateQueue.size(); ++i) {
			Drawable* drawable = updateQueue[i];
			if (drawable) {
				drawable->octant = nullptr;
				drawable->SetFlag(Drawable::FLAG_OCTREE_REINSERT_QUEUED, false);
			}
		}

		DeleteChildOctants(&root, true);
	}

	void Octree::Update(unsigned short frameNumber_)
	{
		frameNumber = frameNumber_;

		// Avoid overhead of threaded update if only a small number of objects to update / reinsert
		if (updateQueue.size()) {
			SetThreadedUpdate(true);

			// Split into smaller tasks to encourage work stealing in case some thread is slower
			size_t nodesPerTask = std::max(MIN_THREADED_UPDATE, updateQueue.size() / workQueue->NumThreads() / 4);
			size_t taskIdx = 0;

			for (size_t start = 0; start < updateQueue.size(); start += nodesPerTask) {
				size_t end = start + nodesPerTask;
				if (end > updateQueue.size()) {
					end = updateQueue.size();
				}

				if (reinsertTasks.size() <= taskIdx) {
					reinsertTasks.push_back(std::make_unique<ReinsertDrawablesTask>(this, &Octree::CheckReinsertWork));
				}
				reinsertTasks[taskIdx]->start = &updateQueue[0] + start;
				reinsertTasks[taskIdx]->end = &updateQueue[0] + end;
				++taskIdx;
			}

			numPendingReinsertionTasks.store((int)taskIdx);
			workQueue->QueueTasks(taskIdx, reinterpret_cast<Task**>(&reinsertTasks[0]));
		} else {
			numPendingReinsertionTasks.store(0);
		}
	}

	void Octree::FinishUpdate()
	{
		// Complete tasks until reinsertions done.
		// There may other tasks going on at the same time
		while (numPendingReinsertionTasks.load() > 0) {
			workQueue->TryComplete();
		}

		SetThreadedUpdate(false);

		// Now reinsert drawables that actually need reinsertion into a different octant
		for (size_t i = 0; i < workQueue->NumThreads(); ++i) {
			ReinsertDrawables(reinsertQueues[i]);
		}

		updateQueue.clear();

		// Sort octants' drawables by address and put lights first
		for (size_t i = 0; i < sortDirtyOctants.size(); ++i) {
			Octant* octant = sortDirtyOctants[i];
			std::sort(octant->drawables.begin(), octant->drawables.end(), CompareDrawables);
			octant->SetFlag(Octant::FLAG_DRAWABLES_SORT_DIRTY, false);
		}

		sortDirtyOctants.clear();
	}

	void Octree::Resize(const BoundingBox& boundingBox, int numLevels)
	{
		// Collect nodes to the root and delete all child octants
		updateQueue.clear();
		std::vector<Drawable*> occluders;

		CollectDrawables(updateQueue, &root);
		DeleteChildOctants(&root, false);

		allocator.Reset();
		root.Initialize(graphics, nullptr, boundingBox, (unsigned char)Clamp(numLevels, 1, MAX_OCTREE_LEVELS), 0);
	}

	void Octree::OnRenderDebug(DebugRenderer* debug)
	{
		root.OnRenderDebug(debug);
	}

#if 0
	void Octree::Raycast(std::vector<RaycastResult>& result, const Ray& ray, unsigned drawableFlags, unsigned viewMask, float maxDistance) const
	{
		result.clear();
		CollectDrawables(result, const_cast<Octant*>(&root), ray, drawableFlags, viewMask, maxDistance);
		std::sort(result.begin(), result.end(), CompareRaycastResults);
	}

	RaycastResult Octree::RaycastSingle(const Ray& ray, unsigned drawableFlags, unsigned viewMask, float maxDistance) const
	{
		// Get the potential hits first
		initialRayResult.clear();
		CollectDrawables(initialRayResult, const_cast<Octant*>(&root), ray, drawableFlags, viewMask, maxDistance);
		std::sort(initialRayResult.begin(), initialRayResult.end(), CompareDrawableDistances);

		// Then perform actual per-node ray tests and early-out when possible
		finalRayResult.clear();
		float closestHit = M_INFINITY;
		for (auto it = initialRayResult.begin(); it != initialRayResult.end(); ++it) {
			if (it->second < std::min(closestHit, maxDistance)) {
				size_t oldSize = finalRayResult.size();
				it->first->OnRaycast(finalRayResult, ray, maxDistance);
				if (finalRayResult.size() > oldSize) {
					closestHit = std::min(closestHit, finalRayResult.back().distance);
				}
			} else {
				break;
			}
		}

		if (finalRayResult.size()) {
			std::sort(finalRayResult.begin(), finalRayResult.end(), CompareRaycastResults);
			return finalRayResult.front();
		} else {
			RaycastResult emptyRes;
			emptyRes.position = emptyRes.normal = Vector3::ZERO();
			emptyRes.distance = M_INFINITY;
			emptyRes.drawable = nullptr;
			emptyRes.subObject = 0;
			return emptyRes;
		}
	}
#endif

	void Octree::QueueUpdate(Drawable* drawable)
	{
		assert(drawable);

		if (drawable->octant) {
			drawable->octant->MarkCullingBoxDirty();
		}

		if (!threadedUpdate) {
			updateQueue.push_back(drawable);
			drawable->SetFlag(Drawable::FLAG_OCTREE_REINSERT_QUEUED, true);
		} else {
			drawable->lastUpdateFrameNumber = frameNumber;

			// Do nothing if still fits the current octant
			const BoundingBox& box = drawable->WorldBoundingBox();
			Octant* oldOctant = drawable->GetOctant();
			if (!oldOctant || oldOctant->fittingBox.IsInside(box) != INSIDE) {
				reinsertQueues[WorkQueue::ThreadIndex()].push_back(drawable);
				drawable->SetFlag(Drawable::FLAG_OCTREE_REINSERT_QUEUED, true);
			}
		}
	}

	void Octree::RemoveDrawable(Drawable* drawable)
	{
		if (!drawable) {
			return;
		}

		RemoveDrawable(drawable, drawable->GetOctant());
		if (drawable->TestFlag(Drawable::FLAG_OCTREE_REINSERT_QUEUED)) {
			RemoveDrawableFromQueue(drawable, updateQueue);

			// Remove also from threaded queues if was left over before next update
			for (size_t i = 0; i < workQueue->NumThreads(); ++i) {
				RemoveDrawableFromQueue(drawable, reinsertQueues[i]);
			}

			drawable->SetFlag(Drawable::FLAG_OCTREE_REINSERT_QUEUED, false);
		}

		drawable->octant = nullptr;
	}

	void Octree::ReinsertDrawables(std::vector<Drawable*>& drawables)
	{
		for (size_t i = 0; i < drawables.size(); ++i) {
			Drawable* drawable = drawables[i];

			const BoundingBox& box = drawable->WorldBoundingBox();
			Octant* oldOctant = drawable->GetOctant();
			Octant* newOctant = &root;
			Vector3 boxSize = box.Size();

			for (;;) {
				// If drawable does not fit fully inside root octant, must remain in it
				bool insertHere = (newOctant == &root) ?
					(newOctant->fittingBox.IsInside(box) != INSIDE || newOctant->FitBoundingBox(box, boxSize)) :
					newOctant->FitBoundingBox(box, boxSize);

				if (insertHere) {
					if (newOctant != oldOctant) {
						// Add first, then remove, because drawable count going to zero deletes the octree branch in question
						AddDrawable(drawable, newOctant);
						if (oldOctant) {
							RemoveDrawable(drawable, oldOctant);
						}
					}
					break;
				} else {
					newOctant = CreateChildOctant(newOctant, newOctant->ChildIndex(box.Center()));
				}
			}

			drawable->SetFlag(Drawable::FLAG_OCTREE_REINSERT_QUEUED, false);
		}

		drawables.clear();
	}

	void Octree::RemoveDrawableFromQueue(Drawable* drawable, std::vector<Drawable*>& drawables)
	{
		for (size_t i = 0; i < drawables.size(); ++i) {
			if (drawables[i] == drawable) {
				drawables[i] = nullptr;
				break;
			}
		}
	}

	void Octree::AddDrawable(Drawable* drawable, Octant* octant)
	{
		octant->drawables.push_back(drawable);
		drawable->octant = octant;
		octant->MarkCullingBoxDirty();

		if (!octant->TestFlag(Octant::FLAG_DRAWABLES_SORT_DIRTY)) {
			octant->SetFlag(Octant::FLAG_DRAWABLES_SORT_DIRTY, true);
			sortDirtyOctants.push_back(octant);
		}
	}

	void Octree::RemoveDrawable(Drawable* drawable, Octant* octant)
	{
		if (!octant) {
			return;
		}

		octant->MarkCullingBoxDirty();

		// Do not set the drawable's octant pointer to zero, as the drawable may already be added into another octant.
		// Just remove from octant
		for (auto it = octant->drawables.begin(); it != octant->drawables.end(); ++it) {
			if ((*it) == drawable) {
				octant->drawables.erase(it);

				// Erase empty octants as necessary, but never the root
				while (!octant->drawables.size() && !octant->numChildren && octant->parent) {
					Octant* parent = octant->parent;
					DeleteChildOctant(parent, octant->childIndex);
					octant = parent;
				}
				return;
			}
		}
	}

	Octant* Octree::CreateChildOctant(Octant* octant, unsigned char index)
	{
		if (octant->children[index]) {
			return octant->children[index];
		}

		// Remove the culling extra from the bounding box before splitting
		Vector3 newMin = octant->fittingBox.min + octant->halfSize;
		Vector3 newMax = octant->fittingBox.max - octant->halfSize;
		const Vector3& oldCenter = octant->center;

		if (index & 1) {
			newMin.x = oldCenter.x;
		} else {
			newMax.x = oldCenter.x;
		}

		if (index & 2) {
			newMin.y = oldCenter.y;
		} else {
			newMax.y = oldCenter.y;
		}

		if (index & 4) {
			newMin.z = oldCenter.z;
		} else {
			newMax.z = oldCenter.z;
		}

		Octant* child = allocator.Allocate();
		child->Initialize(graphics, octant, BoundingBox(newMin, newMax), octant->level - 1, index);
		octant->children[index] = child;
		++octant->numChildren;

		octant->SetFlag(Octant::FLAG_CULLING_BOX_DIRTY, true);

		return child;
	}

	void Octree::DeleteChildOctant(Octant* octant, unsigned char index)
	{
		allocator.Free(octant->children[index]);
		octant->children[index] = nullptr;
		--octant->numChildren;
	}

	void Octree::DeleteChildOctants(Octant* octant, bool deletingOctree)
	{
		std::vector<Drawable*>& drawables = octant->drawables;
		for (size_t i = 0; i < drawables.size(); ++i) {
			Drawable* drawable = drawables[i];
			drawable->octant = nullptr;
			drawable->SetFlag(Drawable::FLAG_OCTREE_REINSERT_QUEUED, false);
			if (deletingOctree) {
				drawable->Owner()->octree = nullptr;
			}
		}
		drawables.clear();

		if (octant->numChildren) {
			for (size_t i = 0; i < NUM_OCTANTS; ++i) {
				if (octant->children[i]) {
					DeleteChildOctants(octant->children[i], deletingOctree);
					allocator.Free(octant->children[i]);
					octant->children[i] = nullptr;
				}
			}
			octant->numChildren = 0;
		}
	}

	void Octree::CollectDrawables(std::vector<Drawable*>& result, Octant* octant) const
	{
		result.insert(result.end(), octant->drawables.begin(), octant->drawables.end());

		if (octant->numChildren) {
			for (size_t i = 0; i < NUM_OCTANTS; ++i) {
				if (octant->children[i]) {
					CollectDrawables(result, octant->children[i]);
				}
			}
		}
	}

	void Octree::CollectDrawables(std::vector<Drawable*>& result, Octant* octant, unsigned drawableFlags, unsigned viewMask) const
	{
		std::vector<Drawable*>& drawables = octant->drawables;
		for (size_t i = 0; i < drawables.size(); ++i) {
			Drawable* drawable = drawables[i];
			if ((drawable->Flags() & drawableFlags) == drawableFlags && (drawable->ViewMask() & viewMask)) {
				result.push_back(drawable);
			}
		}

		if (octant->numChildren) {
			for (size_t i = 0; i < NUM_OCTANTS; ++i) {
				if (octant->children[i]) {
					CollectDrawables(result, octant->children[i], drawableFlags, viewMask);
				}
			}
		}
	}

	void Octree::CollectDrawables(std::vector<RaycastResult>& result, Octant* octant, const Ray& ray, unsigned drawableFlags, unsigned viewMask, float maxDistance) const
	{
		float octantDist = ray.HitDistance(octant->CullingBox());
		if (octantDist >= maxDistance) {
			return;
		}

		std::vector<Drawable*>& drawables = octant->drawables;
		for (size_t i = 0; i < drawables.size(); ++i) {
			Drawable* drawable = drawables[i];
			if ((drawable->Flags() & drawableFlags) == drawableFlags && (drawable->ViewMask() & viewMask)) {
				drawable->OnRaycast(result, ray, maxDistance);
			}
		}

		if (octant->numChildren) {
			for (size_t i = 0; i < NUM_OCTANTS; ++i) {
				if (octant->children[i]) {
					CollectDrawables(result, octant->children[i], ray, drawableFlags, viewMask, maxDistance);
				}
			}
		}
	}

	void Octree::CollectDrawables(std::vector<std::pair<Drawable*, float>>& result, Octant* octant, const Ray& ray, unsigned drawableFlags, unsigned viewMask, float maxDistance) const
	{
		float octantDist = ray.HitDistance(octant->CullingBox());
		if (octantDist >= maxDistance) {
			return;
		}

		std::vector<Drawable*>& drawables = octant->drawables;
		for (size_t i = 0; i < drawables.size(); ++i) {
			Drawable* drawable = drawables[i];
			if ((drawable->Flags() & drawableFlags) == drawableFlags && (drawable->ViewMask() & viewMask)) {
				float distance = ray.HitDistance(drawable->WorldBoundingBox());
				if (distance < maxDistance) {
					result.push_back(std::make_pair(drawable, distance));
				}
			}
		}

		if (octant->numChildren) {
			for (size_t i = 0; i < NUM_OCTANTS; ++i) {
				if (octant->children[i]) {
					CollectDrawables(result, octant->children[i], ray, drawableFlags, viewMask, maxDistance);
				}
			}
		}
	}

	void Octree::CheckReinsertWork(Task* task_, unsigned threadIndex_)
	{
		ReinsertDrawablesTask* task = static_cast<ReinsertDrawablesTask*>(task_);
		Drawable** start = task->start;
		Drawable** end = task->end;
		std::vector<Drawable*>& reinsertQueue = reinsertQueues[threadIndex_];

		for (; start != end; ++start) {
			// If drawable was removed before reinsertion could happen, a null pointer will be in its place
			Drawable* drawable = *start;
			if (!drawable) {
				continue;
			}

			if (drawable->TestFlag(Drawable::FLAG_OCTREE_UPDATE_CALL)) {
				drawable->OnOctreeUpdate(frameNumber);
			}

			drawable->lastUpdateFrameNumber = frameNumber;

			// Do nothing if still fits the current octant
			const BoundingBox& box = drawable->WorldBoundingBox();
			Octant* oldOctant = drawable->GetOctant();
			if (!oldOctant || oldOctant->fittingBox.IsInside(box) != INSIDE) {
				reinsertQueue.push_back(drawable);
			} else {
				drawable->SetFlag(Drawable::FLAG_OCTREE_REINSERT_QUEUED, false);
			}
		}

		numPendingReinsertionTasks.fetch_add(-1);
	}
}
