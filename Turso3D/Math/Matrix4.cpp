#include <Turso3D/Math/Matrix3x4.h>
#include <Turso3D/Math/Matrix4.h>

namespace Turso3D
{
	Matrix4::Matrix4(const Matrix3x4& matrix) :
		m00(matrix.m00), m01(matrix.m01), m02(matrix.m02), m03(matrix.m03),
		m10(matrix.m10), m11(matrix.m11), m12(matrix.m12), m13(matrix.m13),
		m20(matrix.m20), m21(matrix.m21), m22(matrix.m22), m23(matrix.m23),
		m30(0.0f), m31(0.0f), m32(0.0f), m33(1.0f)
	{
	}
}
