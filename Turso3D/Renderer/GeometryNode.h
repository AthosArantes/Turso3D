#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Renderer/OctreeNode.h>
#include <cassert>
#include <vector>
#include <memory>
#include <utility>

namespace Turso3D
{
	class GeometryNode;
	class VertexBuffer;
	class IndexBuffer;
	class ShaderProgram;
	class Material;

	// Description of geometry to be rendered.
	// Scene nodes that render the same object can share these to reduce memory load and allow instancing.
	struct Geometry
	{
		// Last sort key for combined distance and state sorting.
		// Used by Renderer.
		std::pair<unsigned, unsigned> lastSortKey;

		// Geometry vertex buffer.
		std::shared_ptr<VertexBuffer> vertexBuffer;
		// Geometry index buffer.
		std::shared_ptr<IndexBuffer> indexBuffer;
		// Draw range start in GPU buffer.
		// Specifies index start if index buffer defined, vertex start otherwise.
		size_t drawStart;
		// Draw range count.
		// Specifies number of indices if index buffer defined, number of vertices otherwise.
		size_t drawCount;

		// LOD transition distance.
		float lodDistance;
	};

	// Draw call source data with optimal memory storage.
	class SourceBatches
	{
	public:
		void SetNumGeometries(size_t num);
		// Set geometry at index.
		// Geometry pointers are raw pointers for safe LOD level changes on OnPrepareRender() in worker threads;
		// a strong ref to the geometry should be held elsewhere.
		void SetGeometry(size_t index, Geometry* geometry)
		{
			assert(index < data.size() && "Index out of bounds");
			data[index].geometry = geometry;
		}
		Geometry* GetGeometry(size_t index) const
		{
			assert(index < data.size() && "Index out of bounds");
			return data[index].geometry;
		}
		size_t NumGeometries() const noexcept { return data.size(); }

		// Set material at index.
		// Materials hold strong refs and should not be changed from worker threads in OnPrepareRender().
		void SetMaterial(size_t index, const std::shared_ptr<Material>& material)
		{
			assert(index < data.size() && "Index out of bounds");
			data[index].material = material;
		}
		const std::shared_ptr<Material>& GetMaterial(size_t index) const
		{
			assert(index < data.size() && "Index out of bounds");
			return data[index].material;
		}

	private:
		struct GeomMat
		{
			std::shared_ptr<Material> material;
			Geometry* geometry;
		};
		std::vector<GeomMat> data;
	};

	// Base class for drawables that contain geometry to be rendered.
	class GeometryDrawable : public Drawable
	{
		friend class GeometryNode;

	public:
		// Construct.
		GeometryDrawable();

		// Prepare object for rendering.
		// Reset framenumber and calculate distance from camera.
		// Called by Renderer in worker threads.
		// Return false if should not render.
		bool OnPrepareRender(unsigned short frameNumber, Camera* camera) override;
		// Update GPU resources and set uniforms for rendering.
		// Called by Renderer when geometry type is not static.
		virtual void OnRender(ShaderProgram* program, size_t geomIndex);

		// Return the draw call source data for direct access.
		const SourceBatches& Batches() const { return batches; }

	protected:
		// Draw call source data.
		SourceBatches batches;
	};

	// ==========================================================================================
	// Base class for scene nodes that contain geometry to be rendered.
	class GeometryNode : public OctreeNode
	{
	public:
		// Return derived drawable.
		GeometryDrawable* GetDrawable() const { return static_cast<GeometryDrawable*>(drawable); }

		// Set number of geometries.
		void SetNumGeometries(size_t num);
		// Set geometry at index.
		void SetGeometry(size_t index, std::shared_ptr<Geometry> geometry);
		// Set material at every geometry index.
		// Specifying null will use the default material (opaque white.)
		void SetMaterial(std::shared_ptr<Material> material);
		// Set material at geometry index.
		void SetMaterial(size_t index, std::shared_ptr<Material> material);

		// Return number of geometries / batches.
		size_t NumGeometries() const { return GetDrawable()->batches.NumGeometries(); }
		// Return geometry by index.
		Geometry* GetGeometry(size_t index) const { return GetDrawable()->batches.GetGeometry(index); }
		// Return material by geometry index.
		const std::shared_ptr<Material>& GetMaterial(size_t index) const { return GetDrawable()->batches.GetMaterial(index); }
	};
}
