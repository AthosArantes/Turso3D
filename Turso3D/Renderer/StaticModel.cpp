#include <Turso3D/Renderer/StaticModel.h>
#include <Turso3D/Core/Allocator.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Renderer/Camera.h>
#include <Turso3D/Renderer/Model.h>
#include <Turso3D/Renderer/Octree.h>
#include <Turso3D/Resource/ResourceCache.h>

namespace Turso3D
{
	static Vector3 DOT_SCALE(1 / 3.0f, 1 / 3.0f, 1 / 3.0f);

	static Allocator<StaticModelDrawable> drawableAllocator;

	// ==========================================================================================
	StaticModelDrawable::StaticModelDrawable() :
		lodBias(1.0f)
	{
	}

	void StaticModelDrawable::OnWorldBoundingBoxUpdate() const
	{
		if (model) {
			worldBoundingBox = model->LocalBoundingBox().Transformed(WorldTransform());
		} else {
			Drawable::OnWorldBoundingBoxUpdate();
		}
	}

	bool StaticModelDrawable::OnPrepareRender(unsigned short frameNumber, Camera* camera)
	{
		distance = camera->Distance(WorldBoundingBox().Center());

		if (maxDistance > 0.0f && distance > maxDistance) {
			return false;
		}

		lastFrameNumber = frameNumber;

		// If model was last updated long ago, reset update framenumber to illegal
		if (frameNumber - lastUpdateFrameNumber == 0x8000) {
			lastUpdateFrameNumber = 0;
		}

		// Find out the new LOD level if model has LODs
		if (Flags() & Drawable::FLAG_HAS_LOD_LEVELS) {
			float lodDistance = camera->LodDistance(distance, WorldScale().DotProduct(DOT_SCALE), lodBias);
			size_t numGeometries = batches.NumGeometries();

			for (size_t i = 0; i < numGeometries; ++i) {
				const std::vector<std::shared_ptr<Geometry>>& lodGeometries = model->LodGeometries(i);
				if (lodGeometries.size() > 1) {
					size_t j;
					for (j = 1; j < lodGeometries.size(); ++j) {
						if (lodDistance <= lodGeometries[j]->lodDistance) {
							break;
						}
					}
					if (batches.GetGeometry(i) != lodGeometries[j - 1].get()) {
						batches.SetGeometry(i, lodGeometries[j - 1].get());
						lastUpdateFrameNumber = frameNumber;
					}
				}
			}
		}

		return true;
	}

	void StaticModelDrawable::OnRaycast(std::vector<RaycastResult>& dest, const Ray& ray, float maxDistance_)
	{
		if (ray.HitDistance(WorldBoundingBox()) < maxDistance_) {
			RaycastResult res;
			res.distance = M_INFINITY;

			// Perform model raycast in its local space
			const Matrix3x4& transform = WorldTransform();
			Ray localRay = ray.Transformed(transform.Inverse());

			size_t numGeometries = batches.NumGeometries();

			for (size_t i = 0; i < numGeometries; ++i) {
				Geometry* geom = batches.GetGeometry(i);
				float localDistance = geom->HitDistance(localRay, &res.normal);

				if (localDistance < M_INFINITY) {
					// If has a hit, transform it back to world space
					Vector3 hitPosition = transform * (localRay.origin + localDistance * localRay.direction);
					float hitDistance = (hitPosition - ray.origin).Length();

					if (hitDistance < maxDistance_ && hitDistance < res.distance) {
						res.position = hitPosition;
						res.normal = (transform * Vector4(res.normal, 0.0f)).Normalized();
						res.distance = hitDistance;
						res.drawable = this;
						res.subObject = i;
					}
				}
			}

			if (res.distance < maxDistance_) {
				dest.push_back(res);
			}
		}
	}

	// ==========================================================================================
	StaticModel::StaticModel()
	{
		drawable = drawableAllocator.Allocate();
		drawable->SetOwner(this);
	}

	StaticModel::~StaticModel()
	{
		if (drawable) {
			RemoveFromOctree();
			drawableAllocator.Free(static_cast<StaticModelDrawable*>(drawable));
			drawable = nullptr;
		}
	}

	void StaticModel::SetModel(const std::shared_ptr<Model>& model)
	{
		StaticModelDrawable* modelDrawable = static_cast<StaticModelDrawable*>(drawable);

		modelDrawable->model = model;
		modelDrawable->SetFlag(Drawable::FLAG_HAS_LOD_LEVELS, false);

		if (model) {
			SetNumGeometries(model->NumGeometries());
			// Start at LOD level 0
			for (size_t i = 0; i < model->NumGeometries(); ++i) {
				SetGeometry(i, model->GetGeometry(i, 0));
				if (model->NumLodLevels(i) > 1) {
					modelDrawable->SetFlag(Drawable::FLAG_HAS_LOD_LEVELS, true);
				}
			}
		} else {
			SetNumGeometries(0);
		}

		OnBoundingBoxChanged();
	}

	void StaticModel::SetLodBias(float bias)
	{
		StaticModelDrawable* modelDrawable = static_cast<StaticModelDrawable*>(drawable);
		modelDrawable->lodBias = std::max(bias, M_EPSILON);
	}

	const std::shared_ptr<Model>& StaticModel::GetModel() const
	{
		return static_cast<StaticModelDrawable*>(drawable)->model;
	}
}
