#include <Turso3D/Renderer/GeometryNode.h>
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/IndexBuffer.h>
#include <Turso3D/Graphics/VertexBuffer.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Renderer/Camera.h>
#include <Turso3D/Renderer/Material.h>
#include <Turso3D/Resource/ResourceCache.h>

namespace Turso3D
{
	void SourceBatches::SetNumGeometries(size_t count)
	{
		if (count == geometryCount) {
			return;
		}

		if (count <= MaxOptimalGeometryCount) {
			data = &arrayData[0];
			heapData.reset();
		} else {
			for (size_t i = 0; i < arrayData.size(); ++i) {
				arrayData[i].material.reset();
				arrayData[i].geometry = nullptr;
			}

			heapData = std::make_unique<Data[]>(count);
			data = heapData.get();
		}

		for (size_t i = 0; i < count; ++i) {
			data[i].material = Material::GetDefault();
			data[i].geometry = nullptr;
		}
		geometryCount = count;
	}

	// ==========================================================================================
	GeometryDrawable::GeometryDrawable()
	{
		SetFlag(Drawable::FLAG_GEOMETRY, true);
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

	void GeometryDrawable::OnRender(ShaderProgram* program, size_t geomIndex)
	{
	}

	// ==========================================================================================
	void GeometryNode::SetNumGeometries(size_t num)
	{
		GeometryDrawable* drawable = GetDrawable();
		drawable->batches.SetNumGeometries(num);
	}

	void GeometryNode::SetGeometry(size_t index, std::shared_ptr<Geometry> geometry)
	{
		if (!geometry) {
			LOG_ERROR("Can not assign null geometry");
			return;
		}
		GeometryDrawable* drawable = GetDrawable();
		if (index < drawable->batches.NumGeometries()) {
			drawable->batches.SetGeometry(index, geometry.get());
		}
	}

	void GeometryNode::SetMaterial(std::shared_ptr<Material> material)
	{
		GeometryDrawable* drawable = GetDrawable();
		for (size_t i = 0; i < drawable->batches.NumGeometries(); ++i) {
			drawable->batches.SetMaterial(i, material ? material : Material::GetDefault());
		}
	}

	void GeometryNode::SetMaterial(size_t index, std::shared_ptr<Material> material)
	{
		GeometryDrawable* drawable = GetDrawable();
		if (index < drawable->batches.NumGeometries()) {
			drawable->batches.SetMaterial(index, material ? material : Material::GetDefault());
		}
	}
}
