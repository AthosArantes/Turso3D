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
	class IndexBuffer;
	class Material;
	class ShaderProgram;
	class VertexBuffer;

	// Description of geometry to be rendered.
	// Scene nodes that render the same object can share these to reduce memory load and allow instancing.
	struct Geometry
	{
		// Return ray hit distance if has CPU-side data, or infinity if no hit or no data.
		float HitDistance(const Ray& ray, Vector3* outNormal = nullptr) const;

		// Last sort key for combined distance and state sorting.
		// Used by Renderer.
		std::pair<unsigned short, unsigned short> lastSortKey;

		// Geometry vertex buffer.
		std::shared_ptr<VertexBuffer> vertexBuffer;
		// Geometry index buffer.
		std::shared_ptr<IndexBuffer> indexBuffer;
		// Draw range start in GPU buffer.
		// Specifies index start if index buffer defined, vertex start otherwise.
		size_t drawStart = 0;
		// Draw range count.
		// Specifies number of indices if index buffer defined, number of vertices otherwise.
		size_t drawCount = 0;
		// LOD transition distance.
		float lodDistance = 0.0f;

		// TODO: Hull geometry vertices
	};

	// Draw call source data with optimal memory storage. 
	class SourceBatches
	{
	public:
		void SetNumGeometries(size_t num);
		size_t NumGeometries() const noexcept
		{
			return data.size();
		}

		// Set geometry at index.
		// Geometry pointers are raw pointers for safe LOD level changes on OnPrepareRender() in worker threads;
		// a strong ref to the geometry should be held elsewhere.
		void SetGeometry(size_t index, Geometry* geometry)
		{
			assert(index < data.size() && "Index out of bounds");
			data[index].geometry = geometry;
		}

		// Set material at index.
		// Materials hold strong refs and should not be changed from worker threads in OnPrepareRender().
		void SetMaterial(size_t index, const std::shared_ptr<Material>& material)
		{
			assert(index < data.size() && "Index out of bounds");
			data[index].material = material;
		}

		Geometry* GetGeometry(size_t index) const
		{
			assert(index < data.size() && "Index out of bounds");
			return data[index].geometry;
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

		// Return geometry type.
		GeometryType GetGeometryType() const { return (GeometryType)(Flags() & Drawable::FLAG_GEOMETRY_TYPE_BITS); }
		// Return the draw call source data for direct access.
		const SourceBatches& Batches() const { return batches; }

	protected:
		// Draw call source data.
		SourceBatches batches;
	};

	// Base class for scene nodes that contain geometry to be rendered.
	class GeometryNode : public OctreeNode
	{
	public:
		// Set number of geometries.
		void SetNumGeometries(size_t num);
		// Set geometry at index.
		void SetGeometry(size_t index, const std::shared_ptr<Geometry>& geometry);
		// Set material at every geometry index.
		// Specifying null will use the default material (opaque white.)
		void SetMaterial(const std::shared_ptr<Material>& material);
		// Set material at geometry index.
		void SetMaterial(size_t index, const std::shared_ptr<Material>& material);

		// Return geometry type.
		GeometryType GetGeometryType() const { return (GeometryType)(drawable->Flags() & Drawable::FLAG_GEOMETRY_TYPE_BITS); }
		// Return number of geometries / batches.
		size_t NumGeometries() const { return static_cast<GeometryDrawable*>(drawable)->batches.NumGeometries(); }
		// Return geometry by index.
		Geometry* GetGeometry(size_t index) const { return static_cast<GeometryDrawable*>(drawable)->batches.GetGeometry(index); }

		// Return material by geometry index.
		const std::shared_ptr<Material>& GetMaterial(size_t index) const { return static_cast<GeometryDrawable*>(drawable)->batches.GetMaterial(index); }
		// Return the draw call source data for direct access.
		const SourceBatches& Batches() const { return static_cast<GeometryDrawable*>(drawable)->batches; }
	};
}
