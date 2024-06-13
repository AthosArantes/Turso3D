#include "ModelConverter.h"
#include <Turso3D/Graphics/VertexBuffer.h>
#include <Turso3D/IO/FileStream.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Math/BoundingBox.h>
#include <Turso3D/Math/Matrix3x4.h>
#include <Turso3D/Renderer/Model.h>
#include <set>

using namespace Turso3D;

namespace Turso3DUtils
{
	constexpr float BONE_SIZE_THRESHOLD = 0.05f;

	// ==========================================================================================
	// Load-time description of a vertex buffer, to be uploaded on the GPU later.
	struct VertexBufferDesc
	{
		// Vertex declaration.
		std::vector<VertexElement> vertexElements;
		// Number of vertices.
		size_t numVertices;
		// Size of one vertex.
		size_t vertexSize;
		// Vertex data.
		std::unique_ptr<uint8_t[]> vertexData;

		// Position only version of the vertex data, to be retained after load.
		std::shared_ptr<Vector3[]> cpuPositionData;
	};

	// Load-time description of an index buffer, to be uploaded on the GPU later.
	struct IndexBufferDesc
	{
		// Number of indices.
		size_t numIndices;
		// Index data.
		std::unique_ptr<unsigned[]> indexData;
		// Index size.
		//size_t indexSize;

		// Index data to be retained after load.
		std::shared_ptr<unsigned[]> cpuIndexData;
	};

	// Load-time description of a geometry.
	struct GeometryDesc
	{
		// LOD distance.
		float lodDistance;
		// Vertex buffer ref.
		unsigned vbRef;
		// Index buffer ref.
		unsigned ibRef;
		// Draw range start.
		unsigned drawStart;
		// Draw range element count.
		unsigned drawCount;
	};

	// ==========================================================================================
	void ApplyBoneMappings(std::vector<VertexBufferDesc>& vbDescs, std::vector<IndexBufferDesc>& ibDescs, const GeometryDesc& geomDesc, const std::vector<unsigned>& boneMappings, std::set<std::pair<unsigned, unsigned>>& processedVertices)
	{
		size_t blendIndicesOffset = 0;
		bool blendIndicesFound = false;
		const VertexBufferDesc& vbDesc = vbDescs[geomDesc.vbRef];
		for (size_t i = 0; i < vbDesc.vertexElements.size(); ++i) {
			if (vbDesc.vertexElements[i].semantic == SEM_BLENDINDICES) {
				blendIndicesFound = true;
				break;
			} else {
				blendIndicesOffset += ElementTypeSize(vbDesc.vertexElements[i].type);
			}
		}

		if (!blendIndicesFound) {
			return;
		}

		uint8_t* blendIndicesData = vbDesc.vertexData.get() + blendIndicesOffset;

		const IndexBufferDesc& ibDesc = ibDescs[geomDesc.ibRef];

		unsigned* indexData = (unsigned*)ibDesc.indexData.get();
		indexData += geomDesc.drawStart;

		unsigned drawCount = geomDesc.drawCount;
		while (drawCount--) {
			unsigned vIndex = *indexData++;
			std::pair<unsigned, unsigned> vRef {geomDesc.vbRef, vIndex};
			if (processedVertices.find(vRef) != processedVertices.end()) {
				continue;
			}
			for (size_t i = 0; i < 4; ++i) {
				blendIndicesData[vIndex * vbDesc.vertexSize + i] = (unsigned char)boneMappings[blendIndicesData[vIndex * vbDesc.vertexSize + i]];
			}
			processedVertices.insert(vRef);
		}
	}

	// ==========================================================================================
	void ConvertModel(const std::string& src, const std::string& dst)
	{
		FileStream source(src);

		char header[4];
		source.Read(header, 4);

		if (memcmp(header, "UMDL", 4) != 0) {
			LOG_ERROR("{:s} is not a valid model file", source.Name());
			return;
		}

		std::vector<ModelBone> bones;
		std::vector<VertexBufferDesc> vbDescs;
		std::vector<IndexBufferDesc> ibDescs;
		std::vector<std::vector<GeometryDesc>> geomDescs;

		std::vector<unsigned> vbDescElementMask;
		std::vector<unsigned char> vBoneCollisionType;

		unsigned numVertexBuffers = source.Read<unsigned>();
		vbDescs.resize(numVertexBuffers);
		vbDescElementMask.resize(numVertexBuffers);

		for (unsigned i = 0; i < numVertexBuffers; ++i) {
			VertexBufferDesc& vbDesc = vbDescs[i];

			vbDesc.numVertices = source.Read<unsigned>();
			unsigned elementMask = source.Read<unsigned>();
			source.Read<unsigned>(); // morphRangeStart
			source.Read<unsigned>(); // morphRangeCount

			vbDescElementMask[i] = elementMask;

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
			vbDesc.vertexData.reset(new uint8_t[vbDesc.numVertices * vertexSize]);
			source.Read(&vbDesc.vertexData[0], vbDesc.numVertices * vertexSize);
		}

		unsigned numIndexBuffers = source.Read<unsigned>();
		ibDescs.resize(numIndexBuffers);
		for (unsigned i = 0; i < numIndexBuffers; ++i) {
			IndexBufferDesc& ibDesc = ibDescs[i];

			ibDesc.numIndices = source.Read<unsigned>();
			ibDesc.indexData.reset(new unsigned[ibDesc.numIndices]);

			unsigned indexSize = source.Read<unsigned>();
			if (indexSize == sizeof(unsigned short)) {
				// Convert from 16bit indices to 32bit indices

				std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(ibDesc.numIndices * indexSize);
				unsigned short* oldIndices = (unsigned short*)buffer.get();

				source.Read(oldIndices, ibDesc.numIndices * indexSize);
				for (size_t j = 0; j < ibDescs[i].numIndices; ++j) {
					((unsigned*)ibDesc.indexData.get())[j] = (unsigned)oldIndices[j];
				}

			} else {
				assert(indexSize == sizeof(unsigned) && "Indices are not 32bit size.");
				source.Read(&ibDesc.indexData[0], ibDesc.numIndices * sizeof(unsigned));
			}
		}

		unsigned numGeometries = source.Read<unsigned>();
		geomDescs.resize(numGeometries);

		std::vector<std::vector<unsigned>> boneMappings;
		std::set<std::pair<unsigned, unsigned>> processedVertices;
		boneMappings.resize(numGeometries);

		for (unsigned i = 0; i < numGeometries; ++i) {
			// Read bone mappings
			size_t boneMappingCount = source.Read<unsigned>();
			boneMappings[i].resize(boneMappingCount);
			if (boneMappingCount) {
				source.Read(&boneMappings[i][0], boneMappingCount * sizeof(unsigned));
			}

			size_t numLodLevels = source.Read<unsigned>();
			geomDescs[i].resize(numLodLevels);

			for (size_t j = 0; j < numLodLevels; ++j) {
				GeometryDesc& geomDesc = geomDescs[i][j];

				geomDesc.lodDistance = source.Read<float>();
				source.Read<unsigned>(); // Primitive type

				geomDesc.vbRef = source.Read<unsigned>();
				geomDesc.ibRef = source.Read<unsigned>();
				geomDesc.drawStart = source.Read<unsigned>();
				geomDesc.drawCount = source.Read<unsigned>();

				// Apply bone mappings to geometry
				if (boneMappingCount) {
					ApplyBoneMappings(vbDescs, ibDescs, geomDesc, boneMappings[i], processedVertices);
				}
			}
		}

		// Read (skip) morphs
		unsigned numMorphs = source.Read<unsigned>();
		if (numMorphs) {
			LOG_ERROR("Models with vertex morphs are not supported for now");
			return;
		}

		// Read skeleton
		unsigned numBones = source.Read<unsigned>();
		bones.resize(numBones);
		vBoneCollisionType.resize(numBones);
		for (unsigned i = 0; i < numBones; ++i) {
			ModelBone& bone = bones[i];
			bone.name = source.Read<std::string>();
			bone.nameHash = StringHash(bone.name);
			bone.parentIndex = source.Read<unsigned>();
			bone.position = source.Read<Vector3>();
			bone.rotation = source.Read<Quaternion>();
			bone.scale = source.Read<Vector3>();
			bone.offsetMatrix = source.Read<Matrix3x4>();

			unsigned char boneCollisionType = source.Read<unsigned char>();
			vBoneCollisionType[i] = boneCollisionType;

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
		BoundingBox bbox = source.Read<BoundingBox>();

		// -----------------------
		FileStream output(dst, FILE_READWRITE_TRUNCATE);

		char newHeader[4] {'T', 'M', 'F', '\0'};
		output.Write(newHeader, 4);

		output.Write(numVertexBuffers);
		for (unsigned i = 0; i < numVertexBuffers; ++i) {
			VertexBufferDesc& vbDesc = vbDescs[i];

			output.Write<unsigned>(vbDesc.numVertices);
			output.Write(vbDescElementMask[i]);
			output.Write(&vbDesc.vertexData[0], vbDesc.numVertices * vbDesc.vertexSize);
		}

		output.Write(numIndexBuffers);
		for (unsigned i = 0; i < numIndexBuffers; ++i) {
			IndexBufferDesc& ibDesc = ibDescs[i];

			output.Write<unsigned>(ibDesc.numIndices);
			output.Write(&ibDesc.indexData[0], ibDesc.numIndices * sizeof(unsigned));
		}

		output.Write(numGeometries);
		for (unsigned i = 0; i < numGeometries; ++i) {
			unsigned numLodLevels = (unsigned)geomDescs[i].size();
			output.Write(numLodLevels);

			for (size_t j = 0; j < numLodLevels; ++j) {
				GeometryDesc& geomDesc = geomDescs[i][j];

				output.Write(geomDesc.lodDistance);
				output.Write(geomDesc.vbRef);
				output.Write(geomDesc.ibRef);
				output.Write(geomDesc.drawStart);
				output.Write(geomDesc.drawCount);
			}
		}

		output.Write(numBones);
		for (unsigned i = 0; i < numBones; ++i) {
			ModelBone& bone = bones[i];
			output.Write(bone.name);
			output.Write<unsigned>(bone.parentIndex);
			output.Write(bone.position);
			output.Write(bone.rotation);
			output.Write(bone.scale);
			output.Write(bone.offsetMatrix);

			unsigned char boneCollisionType = vBoneCollisionType[i];
			output.Write(boneCollisionType);

			if (boneCollisionType & 1) {
				output.Write(bone.radius);
			}
			if (boneCollisionType & 2) {
				output.Write(bone.boundingBox);
			}
		}

		output.Write(bbox);
	}
}
