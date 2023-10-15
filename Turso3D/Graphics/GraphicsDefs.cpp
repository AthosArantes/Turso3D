#include "GraphicsDefs.h"
#include <Turso3D/Math/Matrix3x4.h>

namespace Turso3D
{
	const char* CullModeNames[] =
	{
		"none",
		"front",
		"back",
		nullptr
	};

	const size_t ElementSizes[] =
	{
		sizeof(int),
		sizeof(float),
		sizeof(Vector2),
		sizeof(Vector3),
		sizeof(Vector4),
		sizeof(unsigned)
	};

	const char* ElementSemanticNames[] =
	{
		"POSITION",
		"NORMAL",
		"TANGENT",
		"TEXCOORD",
		"COLOR",
		"BLENDWEIGHT",
		"BLENDINDICES",
		nullptr
	};

	const char* VertexAttributeNames[] =
	{
		"position",
		"normal",
		"tangent",
		"color",
		"texCoord",
		"texCoord1",
		"texCoord2",
		"texCoord3",
		"texCoord4",
		"texCoord5",
		"blendWeights",
		"blendIndices",
		nullptr
	};

	const char* PresetUniformNames[] =
	{
		"worldMatrix",
		nullptr
	};

	const char* ShaderTypeNames[] =
	{
		"VS",
		"FS",
		nullptr
	};

	const char* BlendModeNames[] =
	{
		"replace",
		"add",
		"multiply",
		"alpha",
		"addAlpha",
		"preMulAlpha",
		"invDestAlpha",
		"subtract",
		"subtractAlpha",
		nullptr
	};

	const char* CompareModeNames[] =
	{
		"never",
		"less",
		"equal",
		"lessEqual",
		"greater",
		"notEqual",
		"greaterEqual",
		"always",
		nullptr
	};
}
