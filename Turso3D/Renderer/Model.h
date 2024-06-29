#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Math/BoundingBox.h>
#include <Turso3D/Math/Quaternion.h>
#include <Turso3D/Math/Matrix3x4.h>
#include <Turso3D/Resource/Resource.h>
#include <memory>
#include <map>
#include <vector>

namespace Turso3D
{
	class IndexBuffer;
	class VertexBuffer;
	class Model;
	struct Geometry;

	// Model bone description.
	struct ModelBone
	{
		// Name.
		std::string name;
		// Name hash.
		StringHash nameHash;
		// Initial position.
		Vector3 position;
		// Initial rotation.
		Quaternion rotation;
		// Initial scale.
		Vector3 scale;
		// Offset matrix for skinning.
		Matrix3x4 offsetMatrix;

		// Collision radius.
		float radius;
		// Collision bounding box.
		BoundingBox boundingBox;

		// Parent bone index.
		// If points to self, is the root bone.
		size_t parentIndex;
		// Whether contributes to bounding boxes.
		bool active;
	};

	// ==========================================================================================
	// Class used to store hull meshes
	class HullGroup
	{
		friend class Model;

		struct MeshInfo
		{
			Vector3* vertices;
			size_t numVertices;
			unsigned* indices;
			unsigned numIndices;
		};

	public:
		// Get the number of hull meshes in this set.
		size_t GetCountMeshes() const { return numMeshes; }

		// Get the first vertex of the specified hull mesh.
		const Vector3* GetVertices(size_t meshIndex) const { return meshes[meshIndex].vertices; }
		// Get the number of vertices of the specified hull mesh.
		size_t GetVertexCount(size_t meshIndex) const { return meshes[meshIndex].numVertices; }

		// Return the indices of the specified hull mesh.
		const unsigned* GetIndices(size_t meshIndex) const { return meshes[meshIndex].indices; }
		// Return the number of indices of the specified hull mesh.
		unsigned GetIndexCount(size_t meshIndex) const { return meshes[meshIndex].numIndices; }

	private:
		void Define(const std::vector<MeshInfo>& srcMeshes);
		void Clear();

	private:
		// The buffer containing all data of all hull meshes.
		std::unique_ptr<uint8_t[]> data;
		// The data for each mesh.
		MeshInfo* meshes;
		// The number of meshes.
		size_t numMeshes;
	};

	// ==========================================================================================
	// Combined vertex and index buffers for static models.
	class CombinedBuffer
	{
	public:
		// Construct with the specified vertex elements.
		CombinedBuffer(const std::vector<VertexElement>& elements);

		// Update vertex data at current position and advance use counter.
		// Return true if data fit the buffer.
		bool FillVertices(size_t numVertices, const void* data);
		// Update index data at current position and advance use counter.
		// Return true if data fit the buffer.
		// Note that index data should be 32-bit.
		bool FillIndices(size_t numIndices, const void* data);

		// Return vertex use count so far.
		size_t UsedVertices() const { return usedVertices; }
		// Return index use count so far.
		size_t UsedIndices() const { return usedIndices; }
		// Return the large vertex buffer.
		std::shared_ptr<VertexBuffer>& GetVertexBuffer() { return vertexBuffer; }
		// Return the large index buffer.
		std::shared_ptr<IndexBuffer>& GetIndexBuffer() { return indexBuffer; }

		// Allocate space from a buffer and return it for use.
		// New buffers will be created as necessary.
		static std::shared_ptr<CombinedBuffer> Allocate(const std::vector<VertexElement>& vertexElements, size_t numVertices, size_t numIndices);

	private:
		// Large vertex buffer.
		std::shared_ptr<VertexBuffer> vertexBuffer;
		// Large index buffer.
		std::shared_ptr<IndexBuffer> indexBuffer;
		// Vertex buffer use count so far.
		size_t usedVertices;
		// Index buffer use count so far.
		size_t usedIndices;
	};

	// ==========================================================================================
	// 3D model resource.
	class Model : public Resource
	{
		struct LoadBuffer;

	public:
		// Construct.
		Model();
		// Destruct.
		~Model();

		// Load model from a stream.
		// Return true on success.
		bool BeginLoad(Stream& source) override;
		// Finalize model loading in the main thread.
		// Return true on success.
		bool EndLoad() override;

		// Set number of geometries.
		void SetNumGeometries(size_t num);
		// Set number of LOD levels in a geometry.
		void SetNumLodLevels(size_t index, size_t num);
		// Set local space bounding box.
		void SetLocalBoundingBox(const BoundingBox& box);
		// Set bone descriptions.
		void SetBones(const std::vector<ModelBone>& bones);

		// Return number of geometries.
		size_t NumGeometries() const { return geometries.size(); }
		// Return number of LOD levels in a geometry.
		size_t NumLodLevels(size_t index) const;
		// Return the geometry at batch index and LOD level.
		const std::shared_ptr<Geometry>& GetGeometry(size_t index, size_t lodLevel) const;
		// Return the LOD geometries at batch index.
		const std::vector<std::shared_ptr<Geometry>>& LodGeometries(size_t index) const { return geometries[index]; }
		// Return the local space bounding box.
		const BoundingBox& LocalBoundingBox() const { return boundingBox; }
		// Return the model's bone descriptions.
		const std::vector<ModelBone>& Bones() const { return bones; }

		// Return the hull meshes group.
		const HullGroup& GetHullGroup() const { return hullGroup; }

	private:
		// Local space bounding box.
		BoundingBox boundingBox;
		// Model's bone descriptions.
		std::vector<ModelBone> bones;
		// Geometry LOD levels.
		std::vector<std::vector<std::shared_ptr<Geometry>>> geometries;
		// Combined buffer if in use.
		std::shared_ptr<CombinedBuffer> combinedBuffer;
		// Hull meshes group.
		HullGroup hullGroup;

		// Temporary buffer used for loading. Internal use only.
		std::unique_ptr<LoadBuffer> loadBuffer;
	};
}
