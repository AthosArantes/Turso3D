#include "OctreeNode.h"
#include <Turso3D/Math/Ray.h>
#include <Turso3D/Scene/Scene.h>
#include <Turso3D/Renderer/Camera.h>
#include <Turso3D/Renderer/DebugRenderer.h>
#include <Turso3D/Renderer/Octree.h>

namespace Turso3D
{
	Drawable::Drawable() :
		owner(nullptr),
		octant(nullptr),
		flags(0),
		layer(LAYER_DEFAULT),
		lastFrameNumber(0),
		lastUpdateFrameNumber(0),
		distance(0.0f),
		maxDistance(0.0f)
	{
		SetFlag(FLAG_BOUNDING_BOX_DIRTY, true);
	}

	Drawable::~Drawable()
	{
	}

	void Drawable::OnWorldBoundingBoxUpdate() const
	{
		// The Drawable base class does not have a defined size, so represent as a point
		worldBoundingBox.Define(WorldPosition());
	}

	void Drawable::OnOctreeUpdate(unsigned short)
	{
	}

	bool Drawable::OnPrepareRender(unsigned short frameNumber, Camera* camera)
	{
		distance = camera->Distance(WorldBoundingBox().Center());

		if (maxDistance > 0.0f && distance > maxDistance) {
			return false;
		}

		lastFrameNumber = frameNumber;
		return true;
	}

	void Drawable::OnRaycast(std::vector<RaycastResult>& dest, const Ray& ray, float maxDistance_)
	{
		float hitDistance = ray.HitDistance(WorldBoundingBox());
		if (hitDistance < maxDistance_) {
			RaycastResult res;
			res.position = ray.origin + hitDistance * ray.direction;
			res.normal = -ray.direction;
			res.distance = hitDistance;
			res.drawable = this;
			res.subObject = 0;
			dest.push_back(res);
		}
	}

	void Drawable::OnRenderDebug(DebugRenderer* debug)
	{
		debug->AddBoundingBox(WorldBoundingBox(), Color::GREEN(), false);
	}

	void Drawable::SetOwner(OctreeNodeBase* owner_)
	{
		owner = owner_;
		worldTransform = const_cast<Matrix3x4*>(&owner_->WorldTransform());
	}

	void Drawable::SetLayer(uint8_t newLayer)
	{
		layer = newLayer;
	}

	// ==========================================================================================
	OctreeNodeBase::OctreeNodeBase() :
		octree(nullptr),
		drawable(nullptr)
	{
	}

	void OctreeNodeBase::OnLayerChanged(uint8_t newLayer)
	{
		if (drawable) {
			drawable->SetLayer(newLayer);
		}
	}

	// ==========================================================================================
	void OctreeNode::SetStatic(bool enable)
	{
		if (enable != IsStatic()) {
			drawable->SetFlag(Drawable::FLAG_STATIC, enable);
			// Reinsert into octree so that cached shadow map invalidation is handled
			OnBoundingBoxChanged();
		}
	}

	void OctreeNode::SetCastShadows(bool enable)
	{
		if (drawable->TestFlag(Drawable::FLAG_CAST_SHADOWS) != enable) {
			drawable->SetFlag(Drawable::FLAG_CAST_SHADOWS, enable);
			// Reinsert into octree so that cached shadow map invalidation is handled
			OnBoundingBoxChanged();
		}
	}

	void OctreeNode::SetUpdateInvisible(bool enable)
	{
		drawable->SetFlag(Drawable::FLAG_UPDATE_INVISIBLE, enable);
	}

	void OctreeNode::SetMaxDistance(float distance_)
	{
		drawable->maxDistance = std::max(distance_, 0.0f);
	}

	void OctreeNode::OnSceneSet(Scene* newScene, Scene*)
	{
		// Remove from current octree if any
		RemoveFromOctree();

		if (newScene) {
			// Octree must be attached to the scene root as a child
			octree = newScene->GetOctree();
			// Transform may not be final yet. Schedule insertion for next octree update
			if (octree && IsEnabled()) {
				octree->QueueUpdate(drawable);
			}
		}
	}

	void OctreeNode::OnTransformChanged()
	{
		SpatialNode::OnTransformChanged();

		drawable->SetFlag(Drawable::FLAG_WORLD_TRANSFORM_DIRTY | Drawable::FLAG_BOUNDING_BOX_DIRTY, true);
		if (drawable->GetOctant() && !drawable->TestFlag(Drawable::FLAG_OCTREE_REINSERT_QUEUED)) {
			octree->QueueUpdate(drawable);
		}
	}

	void OctreeNode::OnBoundingBoxChanged()
	{
		drawable->SetFlag(Drawable::FLAG_BOUNDING_BOX_DIRTY, true);
		if (drawable->GetOctant() && !drawable->TestFlag(Drawable::FLAG_OCTREE_REINSERT_QUEUED)) {
			octree->QueueUpdate(drawable);
		}
	}

	void OctreeNode::RemoveFromOctree()
	{
		if (octree) {
			octree->RemoveDrawable(drawable);
			octree = nullptr;
		}
	}

	void OctreeNode::OnEnabledChanged(bool newEnabled)
	{
		if (octree) {
			if (newEnabled) {
				octree->QueueUpdate(drawable);
			} else {
				octree->RemoveDrawable(drawable);
			}
		}
	}
}
