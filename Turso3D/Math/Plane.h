#pragma once

#include <Turso3D/Math/Matrix3x4.h>

namespace Turso3D
{
	// Surface in three-dimensional space.
	class Plane
	{
	public:
		// Construct undefined.
		Plane()
		{
		}

		// Copy-construct.
		Plane(const Plane& plane) :
			normal(plane.normal),
			absNormal(plane.absNormal),
			d(plane.d)
		{
		}

		// Construct from 3 vertices.
		Plane(const Vector3& v0, const Vector3& v1, const Vector3& v2)
		{
			Define(v0, v1, v2);
		}

		// Construct from a normal vector and a point on the plane.
		Plane(const Vector3& normal, const Vector3& point)
		{
			Define(normal, point);
		}

		// Construct from a 4-dimensional vector, where the w coordinate is the plane parameter.
		Plane(const Vector4& plane)
		{
			Define(plane);
		}

		// Define from 3 vertices.
		void Define(const Vector3& v0, const Vector3& v1, const Vector3& v2)
		{
			Vector3 dist1 = v1 - v0;
			Vector3 dist2 = v2 - v0;

			Define(dist1.CrossProduct(dist2), v0);
		}

		// Define from a normal vector and a point on the plane.
		void Define(const Vector3& normal_, const Vector3& point)
		{
			normal = normal_.Normalized();
			absNormal = normal.Abs();
			d = -normal.DotProduct(point);
		}

		// Define from a 4-dimensional vector, where the w coordinate is the plane parameter.
		void Define(const Vector4& plane)
		{
			normal = Vector3(plane.x, plane.y, plane.z);
			absNormal = normal.Abs();
			d = plane.w;
		}

		// Transform with a 3x3 matrix.
		void Transform(const Matrix3& transform)
		{
			Define(Matrix4(transform).Inverse().Transpose() * ToVector4());
		}

		// Transform with a 3x4 matrix.
		void Transform(const Matrix3x4& transform)
		{
			Define(transform.ToMatrix4().Inverse().Transpose() * ToVector4());
		}

		// Transform with a 4x4 matrix.
		void Transform(const Matrix4& transform)
		{
			Define(transform.Inverse().Transpose() * ToVector4());
		}

		// Project a point on the plane.
		Vector3 Project(const Vector3& point) const
		{
			return point - normal * (normal.DotProduct(point) + d);
		}

		// Return signed distance to a point.
		float Distance(const Vector3& point) const
		{
			return normal.DotProduct(point) + d;
		}

		// Reflect a normalized direction vector.
		Vector3 Reflect(const Vector3& direction) const
		{
			return direction - (2.0f * normal.DotProduct(direction) * normal);
		}

		// Return a reflection matrix.
		Matrix3x4 ReflectionMatrix() const
		{
			return Matrix3x4(
				-2.0f * normal.x * normal.x + 1.0f,
				-2.0f * normal.x * normal.y,
				-2.0f * normal.x * normal.z,
				-2.0f * normal.x * d,
				-2.0f * normal.y * normal.x,
				-2.0f * normal.y * normal.y + 1.0f,
				-2.0f * normal.y * normal.z,
				-2.0f * normal.y * d,
				-2.0f * normal.z * normal.x,
				-2.0f * normal.z * normal.y,
				-2.0f * normal.z * normal.z + 1.0f,
				-2.0f * normal.z * d
			);
		}

		// Return transformed by a 3x3 matrix.
		Plane Transformed(const Matrix3& transform) const
		{
			return Plane(Matrix4(transform).Inverse().Transpose() * ToVector4());
		}

		// Return transformed by a 3x4 matrix.
		Plane Transformed(const Matrix3x4& transform) const
		{
			return Plane(transform.ToMatrix4().Inverse().Transpose() * ToVector4());
		}

		// Return transformed by a 4x4 matrix.
		Plane Transformed(const Matrix4& transform) const
		{
			return Plane(transform.Inverse().Transpose() * ToVector4());
		}

		// Return as a vector.
		Vector4 ToVector4() const
		{
			return Vector4(normal, d);
		}

		// Plane at origin with normal pointing up.
		static Plane UP();

	public:
		// Plane normal.
		Vector3 normal;
		// Plane absolute normal.
		Vector3 absNormal;
		// Plane constant.
		float d;
	};

	// ==========================================================================================
	inline Plane Plane::UP()
	{
		return Plane {Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f)};
	}
}
