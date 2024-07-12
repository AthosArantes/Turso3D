#include <Turso3D/Renderer/Model.h>
#include <Turso3D/Graphics/IndexBuffer.h>
#include <Turso3D/Graphics/VertexBuffer.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/IO/Stream.h>
#include <Turso3D/Renderer/GeometryNode.h>
#include <Turso3D/Renderer/Material.h>
#include <Turso3D/Scene/Node.h>
#include <algorithm>

namespace
{
	using namespace Turso3D;

	// Vertex and index allocation for the combined model buffers
	constexpr size_t COMBINEDBUFFER_VERTICES = 384 * 1024;
	constexpr size_t COMBINEDBUFFER_INDICES = 1024 * 1024;

	// Bone bounding box size required to contribute to bounding box recalculation
	constexpr float BONE_SIZE_THRESHOLD = 0.05f;

	static std::map<size_t, std::vector<std::weak_ptr<CombinedBuffer>>> CombinedBufferMap;
}

namespace Turso3D
{
	void HullGroup::Define(const std::vector<MeshInfo>& srcMeshes)
	{
		size_t sz = 0;
		for (size_t i = 0; i < srcMeshes.size(); ++i) {
			sz += sizeof(Vector3) * srcMeshes[i].numVertices;
			sz += sizeof(unsigned) * srcMeshes[i].numIndices;
		}

		numMeshes = srcMeshes.size();
		data = std::make_unique<uint8_t[]>(sz + sizeof(MeshInfo) * numMeshes);
		meshes = reinterpret_cast<MeshInfo*>(data.get() + sz);

		size_t offset = 0;
		for (size_t i = 0; i < numMeshes; ++i) {
			const MeshInfo& src = srcMeshes[i];
			MeshInfo& dst = meshes[i];

			const size_t vertex_stride = sizeof(Vector3) * src.numVertices;
			const size_t index_stride = sizeof(unsigned) * src.numIndices;

			dst.vertices = reinterpret_cast<Vector3*>(data.get() + offset);
			dst.numVertices = src.numVertices;

			dst.indices = reinterpret_cast<unsigned*>(data.get() + offset + vertex_stride);
			dst.numIndices = src.numIndices;

			memcpy(dst.vertices, src.vertices, vertex_stride);
			memcpy(dst.indices, src.indices, index_stride);

			offset += vertex_stride + index_stride;
		}
	}

	void HullGroup::Clear()
	{
		data.reset();
		meshes = nullptr;
		numMeshes = 0;
	}

	// ==========================================================================================
	struct Model::LoadBuffer
	{
		struct VertexDesc
		{
			// Vertex declaration.
			std::vector<VertexElement> vertexElements;
			// Number of vertices.
			size_t numVertices;
			// Size of one vertex.
			size_t vertexSize;
			// Vertex data.
			std::unique_ptr<uint8_t[]> vertexData;
		};
		std::vector<VertexDesc> vertexBuffers;

		struct IndexDesc
		{
			// Number of indices.
			size_t numIndices;
			// Index data.
			std::unique_ptr<unsigned[]> indexData;
		};
		std::vector<IndexDesc> indexBuffers;

		struct GeometryDesc
		{
			// Vertex buffer index.
			unsigned vbIndex;
			// Index buffer index.
			unsigned ibIndex;
			// Draw range start.
			unsigned drawStart;
			// Draw range element count.
			unsigned drawCount;
			// LOD distance.
			float lodDistance;
		};
		std::vector<std::vector<GeometryDesc>> geometries;
	};

	// ==========================================================================================
	CombinedBuffer::CombinedBuffer(const std::vector<VertexElement>& elements) :
		usedVertices(0),
		usedIndices(0)
	{
		vertexBuffer = std::make_shared<VertexBuffer>();
		vertexBuffer->Define(USAGE_DEFAULT, COMBINEDBUFFER_VERTICES, elements.data(), elements.size());
		indexBuffer = std::make_shared<IndexBuffer>();
		indexBuffer->Define(USAGE_DEFAULT, COMBINEDBUFFER_INDICES, sizeof(unsigned));
	}

	bool CombinedBuffer::FillVertices(size_t numVertices, const void* data)
	{
		if (usedVertices + numVertices > vertexBuffer->NumVertices()) {
			return false;
		}
		vertexBuffer->SetData(usedVertices, numVertices, data);
		usedVertices += numVertices;
		return true;
	}

	bool CombinedBuffer::FillIndices(size_t numIndices, const void* data)
	{
		if (usedIndices + numIndices > indexBuffer->NumIndices()) {
			return false;
		}
		indexBuffer->SetData(usedIndices, numIndices, data);
		usedIndices += numIndices;
		return true;
	}

	std::shared_ptr<CombinedBuffer> CombinedBuffer::Allocate(const std::vector<VertexElement>& elements, size_t numVertices, size_t numIndices)
	{
		size_t key = VertexBuffer::CalculateElementsHash(elements.data(), elements.size());

		auto it = CombinedBufferMap.find(key);
		if (it != CombinedBufferMap.end()) {
			std::vector<std::weak_ptr<CombinedBuffer>>& keyBuffers = it->second;

			for (size_t i = 0; i < keyBuffers.size();) {
				std::shared_ptr<CombinedBuffer> buffer = keyBuffers[i].lock();

				// Clean up expired buffers
				if (!buffer) {
					keyBuffers.erase(keyBuffers.begin() + i);
					continue;
				}

				if (buffer->usedVertices + numVertices <= buffer->vertexBuffer->NumVertices() && buffer->usedIndices + numIndices <= buffer->indexBuffer->NumIndices()) {
					return buffer;
				}
				++i;
			}
		}

		// No existing buffer, make new
		LOG_DEBUG("Creating new combined buffer for attribute mask {:d}", key);

		std::shared_ptr<CombinedBuffer> buffer = std::make_shared<CombinedBuffer>(elements);

#ifdef _DEBUG
		if (it != CombinedBufferMap.end()) {
			for (auto vIt = it->second.begin(); vIt != it->second.end(); ++vIt) {
				std::shared_ptr<CombinedBuffer> prevBuffer = vIt->lock();
				LOG_DEBUG("Previous buffer use {:d}/{:d} {:d}/{:d}", prevBuffer->usedVertices, prevBuffer->vertexBuffer->NumVertices(), prevBuffer->usedIndices, prevBuffer->indexBuffer->NumIndices());
			}
		}
#endif

		CombinedBufferMap[key].push_back(buffer);
		return buffer;
	}

	// ==========================================================================================
	Model::Model()
	{
	}

	Model::~Model()
	{
	}

	bool Model::BeginLoad(Stream& source)
	{
		char header[4];
		source.Read(header, 4);

		if (memcmp(header, "TMF", 4) != 0) {
			LOG_ERROR("{:s} is not a valid model file", source.Name());
			return false;
		}

		loadBuffer.reset(new LoadBuffer());

		// Read vertex buffers
		unsigned numVertexBuffers = source.Read<unsigned>();
		loadBuffer->vertexBuffers.resize(numVertexBuffers);
		for (unsigned i = 0; i < numVertexBuffers; ++i) {
			LoadBuffer::VertexDesc& vbDesc = loadBuffer->vertexBuffers[i];

			vbDesc.numVertices = source.Read<unsigned>();
			unsigned elementMask = source.Read<unsigned>();

			unsigned vertexSize = 0;
			if (elementMask & 1) {
				vbDesc.vertexElements.push_back(VertexElement {ELEM_VECTOR3, ATTR_POSITION});
				vertexSize += sizeof(Vector3);
			}
			if (elementMask & 2) {
				vbDesc.vertexElements.push_back(VertexElement {ELEM_VECTOR3, ATTR_NORMAL});
				vertexSize += sizeof(Vector3);
			}
			if (elementMask & 4) {
				vbDesc.vertexElements.push_back(VertexElement {ELEM_UBYTE4, ATTR_VERTEXCOLOR, true});
				vertexSize += 4;
			}
			if (elementMask & 8) {
				vbDesc.vertexElements.push_back(VertexElement {ELEM_VECTOR2, ATTR_TEXCOORD});
				vertexSize += sizeof(Vector2);
			}
			if (elementMask & 16) {
				vbDesc.vertexElements.push_back(VertexElement {ELEM_VECTOR2, ATTR_TEXCOORD2});
				vertexSize += sizeof(Vector2);
			}
			if (elementMask & 32) {
				//vbDesc.vertexElements.push_back(VertexElement {ELEM_VECTOR3, SEM_TEXCOORD});
				//vertexSize += sizeof(Vector3);
				assert(false);
			}
			if (elementMask & 64) {
				//vbDesc.vertexElements.push_back(VertexElement {ELEM_VECTOR3, SEM_TEXCOORD, 1});
				//vertexSize += sizeof(Vector3);
				assert(false);
			}
			if (elementMask & 128) {
				vbDesc.vertexElements.push_back(VertexElement {ELEM_VECTOR4, ATTR_TANGENT});
				vertexSize += sizeof(Vector4);
			}
			if (elementMask & 256) {
				vbDesc.vertexElements.push_back(VertexElement {ELEM_VECTOR4, ATTR_BLENDWEIGHTS});
				vertexSize += sizeof(Vector4);
			}
			if (elementMask & 512) {
				vbDesc.vertexElements.push_back(VertexElement {ELEM_UBYTE4, ATTR_BLENDINDICES});
				vertexSize += 4;
			}

			vbDesc.vertexSize = vertexSize;
			vbDesc.vertexData = std::make_unique<uint8_t[]>(vbDesc.numVertices * vertexSize);
			source.Read(vbDesc.vertexData.get(), vbDesc.numVertices * vertexSize);
		}

		// Read index buffers
		unsigned numIndexBuffers = source.Read<unsigned>();
		loadBuffer->indexBuffers.resize(numIndexBuffers);
		for (unsigned i = 0; i < numIndexBuffers; ++i) {
			LoadBuffer::IndexDesc& ibDesc = loadBuffer->indexBuffers[i];

			ibDesc.numIndices = source.Read<unsigned>();
			ibDesc.indexData = std::make_unique<unsigned[]>(ibDesc.numIndices);
			source.Read(ibDesc.indexData.get(), ibDesc.numIndices * sizeof(unsigned));
		}

		// Read geometries
		unsigned numGeometries = source.Read<unsigned>();
		loadBuffer->geometries.resize(numGeometries);
		for (unsigned i = 0; i < numGeometries; ++i) {
			unsigned numLodLevels = source.Read<unsigned>();
			loadBuffer->geometries[i].resize(numLodLevels);

			for (unsigned j = 0; j < numLodLevels; ++j) {
				LoadBuffer::GeometryDesc& geomDesc = loadBuffer->geometries[i][j];
				geomDesc.lodDistance = source.Read<float>();
				geomDesc.vbIndex = source.Read<unsigned>();
				geomDesc.ibIndex = source.Read<unsigned>();
				geomDesc.drawStart = source.Read<unsigned>();
				geomDesc.drawCount = source.Read<unsigned>();
			}
		}

		// Read skeleton
		unsigned numBones = source.Read<unsigned>();
		bones.resize(numBones);
		for (unsigned i = 0; i < numBones; ++i) {
			ModelBone& bone = bones[i];
			bone.name = source.Read<std::string>();
			bone.nameHash = StringHash(bone.name);
			bone.parentIndex = source.Read<unsigned>();
			bone.position = source.Read<Vector3>();
			bone.rotation = source.Read<Quaternion>();
			bone.scale = source.Read<Vector3>();
			bone.offsetMatrix = source.Read<Matrix3x4>();
			bone.active = true;

			uint8_t boneCollisionType = source.Read<uint8_t>();
			if (boneCollisionType & 1) {
				bone.radius = source.Read<float>();
				if (bone.radius < BONE_SIZE_THRESHOLD * 0.5f) {
					bone.active = false;
				}
			} else {
				bone.radius = 0.0f;
			}

			if (boneCollisionType & 2) {
				bone.boundingBox = source.Read<BoundingBox>();
				if (bone.boundingBox.Size().Length() < BONE_SIZE_THRESHOLD) {
					bone.active = false;
				}
			} else {
				bone.boundingBox = BoundingBox {0.0f, 0.0f};
			}
		}

		// Read bounding box
		boundingBox = source.Read<BoundingBox>();

		// Read hull meshes
		unsigned numHull = source.IsEof() ? 0 : source.Read<unsigned>();
		if (numHull > 0) {
			std::vector<std::vector<Vector3>> vertices;
			std::vector<std::vector<unsigned>> indices;
			std::vector<HullGroup::MeshInfo> buffer;

			vertices.resize(numHull);
			indices.resize(numHull);
			buffer.resize(numHull);

			for (unsigned i = 0; i < numHull; ++i) {
				unsigned numVertices = source.Read<unsigned>();
				vertices[i].resize(numVertices);
				source.Read(vertices[i].data(), sizeof(Vector3) * numVertices);

				unsigned numIndices = source.Read<unsigned>();
				indices[i].resize(numIndices);
				source.Read(indices[i].data(), sizeof(unsigned) * numIndices);

				buffer[i].vertices = vertices[i].data();
				buffer[i].numVertices = numVertices;
				buffer[i].indices = indices[i].data();
				buffer[i].numIndices = numIndices;
			}

			hullGroup.Define(buffer);

		} else {
			hullGroup.Clear();
		}

		return true;
	}

	bool Model::EndLoad()
	{
		if (!loadBuffer) {
			return false;
		}

		size_t totalIndices = 0;
		for (const LoadBuffer::IndexDesc& ib : loadBuffer->indexBuffers) {
			totalIndices += ib.numIndices;
		}

		bool hasWeights = false;
		for (const LoadBuffer::VertexDesc& vb : loadBuffer->vertexBuffers) {
			for (const VertexElement& elem : vb.vertexElements) {
				if (elem.index == ATTR_BLENDWEIGHTS || elem.index == ATTR_BLENDINDICES) {
					hasWeights = true;
					break;
				}
			}
			if (hasWeights) {
				break;
			}
		}

		// Determine if the model can use a combined buffer
		bool useCombinedBuffer = !hasWeights &&
			(loadBuffer->vertexBuffers.size() == 1) &&
			(loadBuffer->vertexBuffers[0].numVertices < COMBINEDBUFFER_VERTICES) &&
			(totalIndices < COMBINEDBUFFER_INDICES);

		if (useCombinedBuffer) {
			const LoadBuffer::VertexDesc& vbDesc = loadBuffer->vertexBuffers[0];

			combinedBuffer = CombinedBuffer::Allocate(vbDesc.vertexElements, vbDesc.numVertices, totalIndices);

			// Take the current vertex count to be used as offset for the indices, then fill the vertices.
			unsigned vertexStart = (unsigned)combinedBuffer->UsedVertices();
			combinedBuffer->FillVertices(vbDesc.numVertices, vbDesc.vertexData.get());

			// Offset the index values by the current combined buffer vertex count,
			// Store the starting index offset in the combined buffer, then then fill the indices.
			std::vector<size_t> indexStarts;
			indexStarts.resize(loadBuffer->indexBuffers.size());

			for (size_t i = 0; i < loadBuffer->indexBuffers.size(); ++i) {
				LoadBuffer::IndexDesc& ibDesc = loadBuffer->indexBuffers[i];

				unsigned* indexData = ibDesc.indexData.get();
				for (size_t j = 0; j < ibDesc.numIndices; ++j) {
					indexData[j] += vertexStart;
				}

				indexStarts[i] = combinedBuffer->UsedIndices();
				combinedBuffer->FillIndices(ibDesc.numIndices, ibDesc.indexData.get());
			}

			// Set GPU buffers
			geometries.resize(loadBuffer->geometries.size());
			for (size_t i = 0; i < loadBuffer->geometries.size(); ++i) {
				const std::vector<LoadBuffer::GeometryDesc>& lodGeometries = loadBuffer->geometries[i];

				geometries[i].resize(lodGeometries.size());
				for (size_t j = 0; j < lodGeometries.size(); ++j) {
					const LoadBuffer::GeometryDesc& geomDesc = lodGeometries[j];

					std::shared_ptr<Geometry> geom = std::make_shared<Geometry>();
					geom->vertexBuffer = combinedBuffer->GetVertexBuffer();
					geom->indexBuffer = combinedBuffer->GetIndexBuffer();
					geom->drawStart = geomDesc.drawStart + indexStarts[geomDesc.ibIndex];
					geom->drawCount = geomDesc.drawCount;
					geom->lodDistance = geomDesc.lodDistance;

					geometries[i][j].swap(geom);
				}
			}

			loadBuffer.reset();
			return true;
		}

		// Create all vertex buffers
		std::vector<std::shared_ptr<VertexBuffer>> vbs;
		vbs.resize(loadBuffer->vertexBuffers.size());

		for (size_t i = 0; i < loadBuffer->vertexBuffers.size(); ++i) {
			const LoadBuffer::VertexDesc& vbDesc = loadBuffer->vertexBuffers[i];

			std::shared_ptr<VertexBuffer> vb = std::make_shared<VertexBuffer>();
			vb->Define(USAGE_DEFAULT, vbDesc.numVertices, vbDesc.vertexElements.data(), vbDesc.vertexElements.size(), vbDesc.vertexData.get());

			vbs[i].swap(vb);
		}

		// Create all index buffers
		std::vector<std::shared_ptr<IndexBuffer>> ibs;
		ibs.resize(loadBuffer->indexBuffers.size());

		for (size_t i = 0; i < loadBuffer->indexBuffers.size(); ++i) {
			const LoadBuffer::IndexDesc& ibDesc = loadBuffer->indexBuffers[i];

			std::shared_ptr<IndexBuffer> ib = std::make_shared<IndexBuffer>();
			ib->Define(USAGE_DEFAULT, ibDesc.numIndices, sizeof(unsigned), ibDesc.indexData.get());

			ibs[i].swap(ib);
		}

		// Set the buffers for each geometry
		geometries.resize(loadBuffer->geometries.size());
		for (size_t i = 0; i < loadBuffer->geometries.size(); ++i) {
			const std::vector<LoadBuffer::GeometryDesc>& lodGeometries = loadBuffer->geometries[i];

			geometries[i].resize(lodGeometries.size());
			for (size_t j = 0; j < lodGeometries.size(); ++j) {
				const LoadBuffer::GeometryDesc& geomDesc = lodGeometries[j];

				std::shared_ptr<Geometry> geom = std::make_shared<Geometry>();
				geom->vertexBuffer = vbs[geomDesc.vbIndex];
				geom->indexBuffer = ibs[geomDesc.ibIndex];
				geom->drawStart = geomDesc.drawStart;
				geom->drawCount = geomDesc.drawCount;
				geom->lodDistance = geomDesc.lodDistance;

				geometries[i][j].swap(geom);
			}
		}

		loadBuffer.reset();
		return true;
	}

	void Model::SetNumGeometries(size_t num)
	{
		geometries.resize(num);
		// Ensure that each geometry has at least 1 LOD level
		for (size_t i = 0; i < geometries.size(); ++i) {
			if (!geometries[i].size()) {
				SetNumLodLevels(i, 1);
			}
		}
	}

	void Model::SetNumLodLevels(size_t index, size_t num)
	{
		if (index >= geometries.size()) {
			LOG_ERROR("Out of bounds geometry index for setting number of LOD levels");
			return;
		}

		geometries[index].resize(num);

		// Ensure that a valid geometry object exists at each index
		for (auto it = geometries[index].begin(); it != geometries[index].end(); ++it) {
			if (!*it) {
				*it = std::make_shared<Geometry>();
			}
		}
	}

	void Model::SetLocalBoundingBox(const BoundingBox& box)
	{
		boundingBox = box;
	}

	void Model::SetBones(const std::vector<ModelBone>& bones_)
	{
		bones = bones_;
	}

	size_t Model::NumLodLevels(size_t index) const
	{
		return index < geometries.size() ? geometries[index].size() : 0;
	}

	const std::shared_ptr<Geometry>& Model::GetGeometry(size_t index, size_t lodLevel) const
	{
		assert((index < geometries.size() && lodLevel < geometries[index].size()) && "Index out of bounds");
		return geometries[index][lodLevel];
	}
}
