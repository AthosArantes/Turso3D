#include "Model.h"
#include <Turso3D/Graphics/IndexBuffer.h>
#include <Turso3D/Graphics/VertexBuffer.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/IO/Stream.h>
#include <Turso3D/Renderer/GeometryNode.h>
#include <Turso3D/Renderer/Material.h>
#include <Turso3D/Scene/Node.h>
#include <algorithm>

namespace Turso3D
{
	// Vertex and index allocation for the combined model buffers
	constexpr size_t COMBINEDBUFFER_VERTICES = 384 * 1024;
	constexpr size_t COMBINEDBUFFER_INDICES = 1024 * 1024;

	// Bone bounding box size required to contribute to bounding box recalculation
	constexpr float BONE_SIZE_THRESHOLD = 0.05f;

	std::map<unsigned, std::vector<std::weak_ptr<CombinedBuffer>>> CombinedBuffer::buffers;

	// ==========================================================================================
	CombinedBuffer::CombinedBuffer(const std::vector<VertexElement>& elements) :
		usedVertices(0),
		usedIndices(0)
	{
		vertexBuffer = std::make_shared<VertexBuffer>();
		vertexBuffer->Define(USAGE_DEFAULT, COMBINEDBUFFER_VERTICES, elements);
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
		unsigned key = VertexBuffer::CalculateAttributeMask(elements);
		auto it = buffers.find(key);
		if (it != buffers.end()) {
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
		if (it != buffers.end()) {
			for (auto vIt = it->second.begin(); vIt != it->second.end(); ++vIt) {
				std::shared_ptr<CombinedBuffer> prevBuffer = vIt->lock();
				LOG_DEBUG("Previous buffer use {:d}/{:d} {:d}/{:d}", prevBuffer->usedVertices, prevBuffer->vertexBuffer->NumVertices(), prevBuffer->usedIndices, prevBuffer->indexBuffer->NumIndices());
			}
		}
#endif

		buffers[key].push_back(buffer);
		return buffer;
	}

	// ==========================================================================================
	ModelBone::ModelBone() :
		initialPosition(Vector3::ZERO()),
		initialRotation(Quaternion::IDENTITY()),
		initialScale(Vector3::ONE()),
		offsetMatrix(Matrix3x4::IDENTITY()),
		radius(0.0f),
		boundingBox(0.0f, 0.0f),
		parentIndex(0),
		active(true)
	{
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

		vbDescs.clear();
		ibDescs.clear();
		geomDescs.clear();

		// Read vertex buffers
		unsigned numVertexBuffers = source.Read<unsigned>();
		vbDescs.resize(numVertexBuffers);
		for (unsigned i = 0; i < numVertexBuffers; ++i) {
			VertexBufferDesc& vbDesc = vbDescs[i];

			vbDesc.numVertices = source.Read<unsigned>();
			unsigned elementMask = source.Read<unsigned>();

			unsigned vertexSize = 0;
			if (elementMask & 1) {
				vbDesc.vertexElements.push_back(VertexElement(ELEM_VECTOR3, SEM_POSITION));
				vertexSize += sizeof(Vector3);
			}
			if (elementMask & 2) {
				vbDesc.vertexElements.push_back(VertexElement(ELEM_VECTOR3, SEM_NORMAL));
				vertexSize += sizeof(Vector3);
			}
			if (elementMask & 4) {
				vbDesc.vertexElements.push_back(VertexElement(ELEM_UBYTE4, SEM_COLOR));
				vertexSize += 4;
			}
			if (elementMask & 8) {
				vbDesc.vertexElements.push_back(VertexElement(ELEM_VECTOR2, SEM_TEXCOORD));
				vertexSize += sizeof(Vector2);
			}
			if (elementMask & 16) {
				vbDesc.vertexElements.push_back(VertexElement(ELEM_VECTOR2, SEM_TEXCOORD, 1));
				vertexSize += sizeof(Vector2);
			}
			if (elementMask & 32) {
				vbDesc.vertexElements.push_back(VertexElement(ELEM_VECTOR3, SEM_TEXCOORD));
				vertexSize += sizeof(Vector3);
			}
			if (elementMask & 64) {
				vbDesc.vertexElements.push_back(VertexElement(ELEM_VECTOR3, SEM_TEXCOORD, 1));
				vertexSize += sizeof(Vector3);
			}
			if (elementMask & 128) {
				vbDesc.vertexElements.push_back(VertexElement(ELEM_VECTOR4, SEM_TANGENT));
				vertexSize += sizeof(Vector4);
			}
			if (elementMask & 256) {
				vbDesc.vertexElements.push_back(VertexElement(ELEM_VECTOR4, SEM_BLENDWEIGHTS));
				vertexSize += sizeof(Vector4);
			}
			if (elementMask & 512) {
				vbDesc.vertexElements.push_back(VertexElement(ELEM_UBYTE4, SEM_BLENDINDICES));
				vertexSize += 4;
			}

			vbDesc.vertexSize = vertexSize;
			vbDesc.vertexData = std::make_unique<uint8_t[]>(vbDesc.numVertices * vertexSize);
			source.Read(vbDesc.vertexData.get(), vbDesc.numVertices * vertexSize);

			if (elementMask & 1) {
				vbDesc.cpuPositionData = std::make_unique<Vector3[]>(vbDesc.numVertices);
				for (size_t j = 0; j < vbDesc.numVertices; ++j) {
					vbDesc.cpuPositionData[j] = *reinterpret_cast<Vector3*>(vbDesc.vertexData.get() + j * vertexSize);
				}
			}
		}

		// Read index buffers
		unsigned numIndexBuffers = source.Read<unsigned>();
		ibDescs.resize(numIndexBuffers);
		for (unsigned i = 0; i < numIndexBuffers; ++i) {
			IndexBufferDesc& ibDesc = ibDescs[i];

			ibDesc.numIndices = source.Read<unsigned>();
			ibDesc.indexData = std::make_unique<unsigned[]>(ibDesc.numIndices);
			source.Read(&ibDesc.indexData[0], ibDesc.numIndices * sizeof(unsigned));

			ibDesc.cpuIndexData = std::make_unique<unsigned[]>(ibDesc.numIndices);
			memcpy(ibDesc.cpuIndexData.get(), ibDesc.indexData.get(), ibDesc.numIndices * sizeof(unsigned));
		}

		// Read geometries
		unsigned numGeometries = source.Read<unsigned>();
		geomDescs.resize(numGeometries);
		for (unsigned i = 0; i < numGeometries; ++i) {
			unsigned numLodLevels = source.Read<unsigned>();
			geomDescs[i].resize(numLodLevels);

			for (unsigned j = 0; j < numLodLevels; ++j) {
				GeometryDesc& geomDesc = geomDescs[i][j];
				geomDesc.lodDistance = source.Read<float>();
				geomDesc.vbRef = source.Read<unsigned>();
				geomDesc.ibRef = source.Read<unsigned>();
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
			bone.initialPosition = source.Read<Vector3>();
			bone.initialRotation = source.Read<Quaternion>();
			bone.initialScale = source.Read<Vector3>();
			bone.offsetMatrix = source.Read<Matrix3x4>();

			unsigned char boneCollisionType = source.Read<unsigned char>();
			if (boneCollisionType & 1) {
				bone.radius = source.Read<float>();
				if (bone.radius < BONE_SIZE_THRESHOLD * 0.5f) {
					bone.active = false;
				}
			}
			if (boneCollisionType & 2) {
				bone.boundingBox = source.Read<BoundingBox>();
				if (bone.boundingBox.Size().Length() < BONE_SIZE_THRESHOLD) {
					bone.active = false;
				}
			}
		}

		// Read bounding box
		boundingBox = source.Read<BoundingBox>();

		return true;
	}

	bool Model::EndLoad()
	{
		bool hasWeights = false;
		size_t totalIndices = 0;

		for (size_t i = 0; i < ibDescs.size(); ++i) {
			totalIndices += ibDescs[i].numIndices;
		}

		for (auto it = vbDescs.begin(); it != vbDescs.end(); ++it) {
			for (auto vIt = it->vertexElements.begin(); vIt != it->vertexElements.end(); ++vIt) {
				if (vIt->semantic == SEM_BLENDWEIGHTS || vIt->semantic == SEM_BLENDINDICES) {
					hasWeights = true;
					break;
				}
			}
			if (hasWeights) {
				break;
			}
		}

		// Create the geometry structure early and fill the CPU-side data first.
		// Fill actual vertex and index buffers later
		geometries.resize(geomDescs.size());
		for (size_t i = 0; i < geomDescs.size(); ++i) {
			geometries[i].resize(geomDescs[i].size());
			for (size_t j = 0; j < geomDescs[i].size(); ++j) {
				const GeometryDesc& geomDesc = geomDescs[i][j];

				std::shared_ptr<Geometry> geom = std::make_shared<Geometry>();
				geom->lodDistance = geomDesc.lodDistance;
				geom->drawStart = geomDesc.drawStart;
				geom->drawCount = geomDesc.drawCount;

				geom->cpuPositionData = vbDescs[geomDesc.vbRef].cpuPositionData;
				geom->cpuIndexData = ibDescs[geomDesc.ibRef].cpuIndexData;
				//geom->cpuIndexSize = ibDescs[geomDesc.ibRef].indexSize;
				geom->cpuDrawStart = geomDesc.drawStart;

				geometries[i][j].swap(geom);
			}
		}

		// Check if can use combined vertex / index buffers
		if (vbDescs.size() == 1 && vbDescs[0].numVertices < COMBINEDBUFFER_VERTICES && totalIndices < COMBINEDBUFFER_INDICES && !hasWeights) {
			combinedBuffer = CombinedBuffer::Allocate(vbDescs[0].vertexElements, vbDescs[0].numVertices, totalIndices);
			unsigned vertexStart = (unsigned)combinedBuffer->UsedVertices();

			// Offset the index values by the current combined buffer vertex count.
			for (size_t i = 0; i < ibDescs.size(); ++i) {
				IndexBufferDesc& ibDesc = ibDescs[i];
				unsigned* indexData = ibDesc.indexData.get();
				for (size_t j = 0; j < ibDescs[i].numIndices; ++j) {
					indexData[j] += vertexStart;
				}
			}

			// Store the starting index offset in the combined buffer.
			std::vector<size_t> indexStarts;
			indexStarts.resize(ibDescs.size());

			// Fill GPU buffers
			combinedBuffer->FillVertices(vbDescs[0].numVertices, vbDescs[0].vertexData.get());
			for (size_t i = 0; i < ibDescs.size(); ++i) {
				indexStarts[i] = combinedBuffer->UsedIndices();
				combinedBuffer->FillIndices(ibDescs[i].numIndices, ibDescs[i].indexData.get());
			}

			// Set GPU buffers
			for (size_t i = 0; i < geomDescs.size(); ++i) {
				for (size_t j = 0; j < geomDescs[i].size(); ++j) {
					const GeometryDesc& geomDesc = geomDescs[i][j];
					Geometry* geom = geometries[i][j].get();

					geom->vertexBuffer = combinedBuffer->GetVertexBuffer();
					geom->indexBuffer = combinedBuffer->GetIndexBuffer();
					geom->drawStart = geomDesc.drawStart + indexStarts[geomDesc.ibRef];
				}
			}

			std::vector<VertexBufferDesc>().swap(vbDescs);
			std::vector<IndexBufferDesc>().swap(ibDescs);
			std::vector<std::vector<GeometryDesc>>().swap(geomDescs);

			return true;
		}

		// If not, create individual buffers for this model and set them to the geometries
		std::vector<std::shared_ptr<VertexBuffer>> vbs;
		for (size_t i = 0; i < vbDescs.size(); ++i) {
			std::shared_ptr<VertexBuffer>& vb = vbs.emplace_back(std::make_shared<VertexBuffer>());

			const VertexBufferDesc& vbDesc = vbDescs[i];
			vb->Define(USAGE_DEFAULT, vbDesc.numVertices, vbDesc.vertexElements, vbDesc.vertexData.get());
		}

		std::vector<std::shared_ptr<IndexBuffer>> ibs;
		for (size_t i = 0; i < ibDescs.size(); ++i) {
			std::shared_ptr<IndexBuffer>& ib = ibs.emplace_back(std::make_shared<IndexBuffer>());

			const IndexBufferDesc& ibDesc = ibDescs[i];
			ib->Define(USAGE_DEFAULT, ibDesc.numIndices, sizeof(unsigned), ibDesc.indexData.get());
		}

		geometries.resize(geomDescs.size());
		for (size_t i = 0; i < geomDescs.size(); ++i) {
			geometries[i].resize(geomDescs[i].size());
			for (size_t j = 0; j < geomDescs[i].size(); ++j) {
				const GeometryDesc& geomDesc = geomDescs[i][j];
				Geometry* geom = geometries[i][j].get();

				geom->vertexBuffer = vbs[geomDesc.vbRef];
				geom->indexBuffer = ibs[geomDesc.ibRef];
			}
		}

		std::vector<VertexBufferDesc>().swap(vbDescs);
		std::vector<IndexBufferDesc>().swap(ibDescs);
		std::vector<std::vector<GeometryDesc>>().swap(geomDescs);

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
