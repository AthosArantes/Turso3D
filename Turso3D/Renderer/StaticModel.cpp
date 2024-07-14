#include <Turso3D/Renderer/StaticModel.h>
#include <Turso3D/Core/Allocator.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Renderer/Camera.h>
#include <Turso3D/Renderer/DebugRenderer.h>
#include <Turso3D/Renderer/Model.h>
#include <Turso3D/Renderer/Octree.h>
#include <Turso3D/Resource/ResourceCache.h>

namespace
{
	using namespace Turso3D;

	static Vector3 DOT_SCALE(1 / 3.0f, 1 / 3.0f, 1 / 3.0f);

	static Allocator<StaticModelDrawable> drawableAllocator;
}

namespace Turso3D
{
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

			const HullGroup& hull = model->GetHullGroup();

			size_t numHulls = hull.GetCountMeshes();
			if (numHulls == 0) {
				return;
			}

			float localDistance = M_INFINITY;
			if (numHulls == 1) {
				localDistance = localRay.HitDistance(hull.GetVertices(0), sizeof(Vector3), hull.GetIndices(0), sizeof(unsigned), 0, hull.GetIndexCount(0), &res.normal);
			} else {
				for (size_t i = 0; i < numHulls; ++i) {
					Vector3 n;
					float d = localRay.HitDistance(hull.GetVertices(i), sizeof(Vector3), hull.GetIndices(i), sizeof(unsigned), 0, hull.GetIndexCount(i), &n);
					if (d < localDistance) {
						localDistance = d;
						res.normal = n;
					}
				}
			}

			if (localDistance < M_INFINITY) {
				// If has a hit, transform it back to world space
				Vector3 hitPosition = transform * (localRay.origin + localDistance * localRay.direction);
				float hitDistance = (hitPosition - ray.origin).Length();

				if (hitDistance < maxDistance_ && hitDistance < res.distance) {
					res.position = hitPosition;
					res.normal = (transform * Vector4(res.normal, 0.0f)).Normalized();
					res.distance = hitDistance;
					res.drawable = this;
					res.subObject = 0; // TODO: Hull mesh index?
				}
			}

			if (res.distance < maxDistance_) {
				dest.push_back(res);
			}
		}
	}

	void StaticModelDrawable::OnRenderDebug(DebugRenderer* renderer)
	{
		renderer->AddBoundingBox(WorldBoundingBox(), Color::GREEN(), false);

		const HullGroup& hull = model->GetHullGroup();
		const Matrix3x4& transform = WorldTransform();

		for (size_t i = 0; i < hull.GetCountMeshes(); ++i) {
			const Vector3* vertices = hull.GetVertices(i);
			const unsigned* indices = hull.GetIndices(i);
			for (size_t j = 0; j < hull.GetIndexCount(i); j += 3) {
				renderer->AddLine(transform * vertices[indices[j]], transform * vertices[indices[j + 1]], Color::MAGENTA());
				renderer->AddLine(transform * vertices[indices[j + 1]], transform * vertices[indices[j + 2]], Color::MAGENTA());
				renderer->AddLine(transform * vertices[indices[j + 2]], transform * vertices[indices[j]], Color::MAGENTA());
			}
		}
	}

	// ==========================================================================================
	StaticModel::StaticModel(Drawable* drawable)
	{
		this->drawable = drawable;
		drawable->SetOwner(this);
	}

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

	void StaticModel::SetModel(std::shared_ptr<Model> model)
	{
		StaticModelDrawable* drawable = GetDrawable();

		drawable->model = model;
		drawable->SetFlag(Drawable::FLAG_HAS_LOD_LEVELS, false);

		if (model) {
			SetNumGeometries(model->NumGeometries());
			// Start at LOD level 0
			for (size_t i = 0; i < model->NumGeometries(); ++i) {
				SetGeometry(i, model->GetGeometry(i, 0));
				if (model->NumLodLevels(i) > 1) {
					drawable->SetFlag(Drawable::FLAG_HAS_LOD_LEVELS, true);
				}
			}
		} else {
			SetNumGeometries(0);
		}

		OnBoundingBoxChanged();
	}

	void StaticModel::SetLodBias(float bias)
	{
		StaticModelDrawable* drawable = GetDrawable();
		drawable->lodBias = std::max(bias, M_EPSILON);
	}
}
