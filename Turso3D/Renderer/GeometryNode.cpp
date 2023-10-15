#include "GeometryNode.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/IndexBuffer.h>
#include <Turso3D/Graphics/VertexBuffer.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Renderer/Camera.h>
#include <Turso3D/Renderer/Material.h>
#include <Turso3D/Resource/ResourceCache.h>

namespace Turso3D
{
	void SourceBatches::SetNumGeometries(size_t num)
	{
		if (num == 0) {
			std::vector<GeomMat>().swap(data);
			return;
		}

		data.resize(num);
		for (size_t i = 0; i < num; ++i) {
			data[i].material = Material::DefaultMaterial();
			data[i].geometry = nullptr;
		}
	}

	// ==========================================================================================
	float Geometry::HitDistance(const Ray& ray, Vector3* outNormal) const
	{
		if (!cpuPositionData) {
			return M_INFINITY;
		}

		if (cpuIndexData) {
			return ray.HitDistance(cpuPositionData.get(), sizeof(Vector3), cpuIndexData.get(), sizeof(unsigned), cpuDrawStart, drawCount, outNormal);
		} else {
			return ray.HitDistance(cpuPositionData.get(), sizeof(Vector3), cpuDrawStart, drawCount, outNormal);
		}
	}

	// ==========================================================================================
	GeometryDrawable::GeometryDrawable()
	{
		SetFlag(DF_GEOMETRY, true);
	}

	bool GeometryDrawable::OnPrepareRender(unsigned short frameNumber, Camera* camera)
	{
		distance = camera->Distance(WorldPosition());
		if (maxDistance > 0.0f && distance > maxDistance) {
			return false;
		}
		lastFrameNumber = frameNumber;
		return true;
	}

	void GeometryDrawable::OnRender(ShaderProgram*, size_t)
	{
	}

	// ==========================================================================================
	void GeometryNode::SetNumGeometries(size_t num)
	{
		GeometryDrawable* geomDrawable = static_cast<GeometryDrawable*>(drawable);
		geomDrawable->batches.SetNumGeometries(num);
	}

	void GeometryNode::SetGeometry(size_t index, const std::shared_ptr<Geometry>& geometry)
	{
		if (!geometry) {
			LOG_ERROR("Can not assign null geometry");
			return;
		}
		GeometryDrawable* geomDrawable = static_cast<GeometryDrawable*>(drawable);
		if (index < geomDrawable->batches.NumGeometries()) {
			geomDrawable->batches.SetGeometry(index, geometry);
		}
	}

	void GeometryNode::SetMaterial(const std::shared_ptr<Material>& material)
	{
		GeometryDrawable* geomDrawable = static_cast<GeometryDrawable*>(drawable);
		for (size_t i = 0; i < geomDrawable->batches.NumGeometries(); ++i) {
			geomDrawable->batches.SetMaterial(i, material ? material : Material::DefaultMaterial());
		}
	}

	void GeometryNode::SetMaterial(size_t index, const std::shared_ptr<Material>& material)
	{
		GeometryDrawable* geomDrawable = static_cast<GeometryDrawable*>(drawable);
		if (index < geomDrawable->batches.NumGeometries()) {
			geomDrawable->batches.SetMaterial(index, material ? material : Material::DefaultMaterial());
		}
	}
}
