#pragma once

#include <Turso3D/Math/BoundingBox.h>
#include <Turso3D/Math/Frustum.h>
#include <Turso3D/Math/Plane.h>
#include <Turso3D/Math/Sphere.h>
#include <Turso3D/Math/Vector3.h>
#include <Turso3D/Math/Matrix3x4.h>

namespace Turso3D
{
	// Infinite straight line in three-dimensional space.
	class Ray
	{
	public:
		// Construct undefined ray.
		Ray()
		{
		}

		// Copy-construct.
		Ray(const Ray& ray) :
			origin(ray.origin),
			direction(ray.direction)
		{
		}

		// Construct from origin and direction.
		// The direction will be normalized.
		Ray(const Vector3& origin, const Vector3& direction)
		{
			Define(origin, direction);
		}

		// Assign from another ray.
		Ray& operator = (const Ray& rhs)
		{
			origin = rhs.origin;
			direction = rhs.direction;
			return *this;
		}

		// Check for equality with another ray without epsilon.
		bool operator == (const Ray& rhs) const
		{
			return origin == rhs.origin && direction == rhs.direction;
		}
		// Check for inequality with another ray without epsilon.
		bool operator != (const Ray& rhs) const
		{
			return !(*this == rhs);
		}

		// Define from origin and direction.
		// The direction will be normalized.
		void Define(const Vector3& origin_, const Vector3& direction_)
		{
			origin = origin_;
			direction = direction_.Normalized();
		}

		// Project a point on the ray.
		Vector3 Project(const Vector3& point) const
		{
			Vector3 offset = point - origin;
			return origin + offset.DotProduct(direction) * direction;
		}

		// Return distance of a point from the ray.
		float Distance(const Vector3& point) const
		{
			Vector3 projected = Project(point);
			return (point - projected).Length();
		}

		// Test for equality with another ray with epsilon.
		bool Equals(const Ray& ray) const
		{
			return origin.Equals(ray.origin) && direction.Equals(ray.direction);
		}

		// Return closest point to another ray.
		Vector3 ClosestPoint(const Ray& ray) const
		{
			// Algorithm based on http://paulbourke.net/geometry/lineline3d/
			Vector3 p13 = origin - ray.origin;
			Vector3 p43 = ray.direction;
			Vector3 p21 = direction;

			float d1343 = p13.DotProduct(p43);
			float d4321 = p43.DotProduct(p21);
			float d1321 = p13.DotProduct(p21);
			float d4343 = p43.DotProduct(p43);
			float d2121 = p21.DotProduct(p21);

			float d = d2121 * d4343 - d4321 * d4321;
			if (std::abs(d) < M_EPSILON) {
				return origin;
			}
			float n = d1343 * d4321 - d1321 * d4343;
			float a = n / d;

			return origin + a * direction;
		}

		// Return hit distance to a plane, or infinity if no hit.
		float HitDistance(const Plane& plane) const
		{
			float d = plane.normal.DotProduct(direction);
			if (std::abs(d) >= M_EPSILON) {
				float t = -(plane.normal.DotProduct(origin) + plane.d) / d;
				if (t >= 0.0f) {
					return t;
				} else {
					return M_INFINITY;
				}
			} else {
				return M_INFINITY;
			}
		}

		// Return hit distance to a bounding box, or infinity if no hit.
		float HitDistance(const BoundingBox& box) const
		{
			// Check for ray origin being inside the box
			if (box.IsInside(origin)) {
				return 0.0f;
			}

			float dist = M_INFINITY;

			// Check for intersecting in the X-direction
			if (origin.x < box.min.x && direction.x > 0.0f) {
				float x = (box.min.x - origin.x) / direction.x;
				if (x < dist) {
					Vector3 point = origin + x * direction;
					if (point.y >= box.min.y && point.y <= box.max.y && point.z >= box.min.z && point.z <= box.max.z) {
						dist = x;
					}
				}
			}
			if (origin.x > box.max.x && direction.x < 0.0f) {
				float x = (box.max.x - origin.x) / direction.x;
				if (x < dist) {
					Vector3 point = origin + x * direction;
					if (point.y >= box.min.y && point.y <= box.max.y && point.z >= box.min.z && point.z <= box.max.z) {
						dist = x;
					}
				}
			}
			// Check for intersecting in the Y-direction
			if (origin.y < box.min.y && direction.y > 0.0f) {
				float x = (box.min.y - origin.y) / direction.y;
				if (x < dist) {
					Vector3 point = origin + x * direction;
					if (point.x >= box.min.x && point.x <= box.max.x && point.z >= box.min.z && point.z <= box.max.z) {
						dist = x;
					}
				}
			}
			if (origin.y > box.max.y && direction.y < 0.0f) {
				float x = (box.max.y - origin.y) / direction.y;
				if (x < dist) {
					Vector3 point = origin + x * direction;
					if (point.x >= box.min.x && point.x <= box.max.x && point.z >= box.min.z && point.z <= box.max.z) {
						dist = x;
					}
				}
			}
			// Check for intersecting in the Z-direction
			if (origin.z < box.min.z && direction.z > 0.0f) {
				float x = (box.min.z - origin.z) / direction.z;
				if (x < dist) {
					Vector3 point = origin + x * direction;
					if (point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y && point.y <= box.max.y) {
						dist = x;
					}
				}
			}
			if (origin.z > box.max.z && direction.z < 0.0f) {
				float x = (box.max.z - origin.z) / direction.z;
				if (x < dist) {
					Vector3 point = origin + x * direction;
					if (point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y && point.y <= box.max.y) {
						dist = x;
					}
				}
			}

			return dist;
		}

		// Return hit distance to a frustum, or infinity if no hit.
		// If solidInside parameter is true (default) rays originating from inside return zero distance, otherwise the distance to the closest plane.
		float HitDistance(const Frustum& frustum, bool solidInside = true) const
		{
			float maxOutside = 0.0f;
			float minInside = M_INFINITY;
			bool allInside = true;

			for (size_t i = 0; i < NUM_FRUSTUM_PLANES; ++i) {
				const Plane& plane = frustum.planes[i];
				float distance = HitDistance(frustum.planes[i]);

				if (plane.Distance(origin) < 0.0f) {
					maxOutside = std::max(maxOutside, distance);
					allInside = false;
				} else {
					minInside = std::min(minInside, distance);
				}
			}

			if (allInside) {
				return solidInside ? 0.0f : minInside;
			} else if (maxOutside <= minInside) {
				return maxOutside;
			} else {
				return M_INFINITY;
			}
		}

		// Return hit distance to a sphere, or infinity if no hit.
		float HitDistance(const Sphere& sphere) const
		{
			Vector3 centeredOrigin = origin - sphere.center;
			float squaredRadius = sphere.radius * sphere.radius;

			// Check if ray originates inside the sphere
			if (centeredOrigin.LengthSquared() <= squaredRadius) {
				return 0.0f;
			}

			// Calculate intersection by quadratic equation
			float a = direction.DotProduct(direction);
			float b = 2.0f * centeredOrigin.DotProduct(direction);
			float c = centeredOrigin.DotProduct(centeredOrigin) - squaredRadius;
			float d = b * b - 4.0f * a * c;

			// No solution
			if (d < 0.0f) {
				return M_INFINITY;
			}

			// Get the nearer solution
			float dSqrt = sqrtf(d);
			float dist = (-b - dSqrt) / (2.0f * a);
			if (dist >= 0.0f) {
				return dist;
			} else {
				return (-b + dSqrt) / (2.0f * a);
			}
		}

		// Return hit distance to a triangle and optionally normal, or infinity if no hit.
		float HitDistance(const Vector3& v0, const Vector3& v1, const Vector3& v2, Vector3* outNormal = nullptr) const
		{
			// Based on Fast, Minimum Storage Ray/Triangle Intersection by Möller & Trumbore
			// http://www.graphics.cornell.edu/pubs/1997/MT97.pdf
			// Calculate edge vectors
			Vector3 edge1(v1 - v0);
			Vector3 edge2(v2 - v0);

			// Calculate determinant & check backfacing
			Vector3 p(direction.CrossProduct(edge2));
			float det = edge1.DotProduct(p);
			if (det >= M_EPSILON) {
				// Calculate u & v parameters and test
				Vector3 t(origin - v0);
				float u = t.DotProduct(p);
				if (u >= 0.0f && u <= det) {
					Vector3 q(t.CrossProduct(edge1));
					float v = direction.DotProduct(q);
					if (v >= 0.0f && u + v <= det) {
						float distance = edge2.DotProduct(q) / det;
						if (distance >= 0.0f) {
							// There is an intersection, so calculate distance & optional normal
							if (outNormal) {
								*outNormal = edge1.CrossProduct(edge2);
							}
							return distance;
						}
					}
				}
			}

			return M_INFINITY;
		}

		// Return hit distance to non-indexed geometry data, or infinity if no hit.
		// Optionally return normal.
		float HitDistance(const void* vertexData, size_t vertexSize, size_t vertexStart, size_t vertexCount, Vector3* outNormal = nullptr) const
		{
			float nearest = M_INFINITY;
			const unsigned char* vertices = ((const unsigned char*)vertexData) + vertexStart * vertexSize;
			size_t index = 0;

			while (index + 2 < vertexCount) {
				const Vector3& v0 = *((const Vector3*)(&vertices[index * vertexSize]));
				const Vector3& v1 = *((const Vector3*)(&vertices[(index + 1) * vertexSize]));
				const Vector3& v2 = *((const Vector3*)(&vertices[(index + 2) * vertexSize]));
				nearest = std::min(nearest, HitDistance(v0, v1, v2, outNormal));
				index += 3;
			}

			return nearest;
		}

		// Return hit distance to indexed geometry data, or infinity if no hit.
		float HitDistance(const void* vertexData, size_t vertexSize, const void* indexData, size_t indexSize, size_t indexStart, size_t indexCount, Vector3* outNormal = nullptr) const
		{
			float nearest = M_INFINITY;
			const unsigned char* vertices = (const unsigned char*)vertexData;

			const unsigned* indices = ((const unsigned*)indexData) + indexStart;
			const unsigned* indicesEnd = indices + indexCount;

			while (indices < indicesEnd) {
				const Vector3& v0 = *((const Vector3*)(&vertices[indices[0] * vertexSize]));
				const Vector3& v1 = *((const Vector3*)(&vertices[indices[1] * vertexSize]));
				const Vector3& v2 = *((const Vector3*)(&vertices[indices[2] * vertexSize]));
				nearest = std::min(nearest, HitDistance(v0, v1, v2, outNormal));
				indices += 3;
			}

			return nearest;
		}

		// Return whether ray is inside non-indexed geometry.
		bool InsideGeometry(const void* vertexData, size_t vertexSize, size_t vertexStart, size_t vertexCount) const
		{
			float currentFrontFace = M_INFINITY;
			float currentBackFace = M_INFINITY;
			const unsigned char* vertices = ((const unsigned char*)vertexData) + vertexStart * vertexSize;
			size_t index = 0;

			while (index + 2 < vertexCount) {
				const Vector3& v0 = *((const Vector3*)(&vertices[index * vertexSize]));
				const Vector3& v1 = *((const Vector3*)(&vertices[(index + 1) * vertexSize]));
				const Vector3& v2 = *((const Vector3*)(&vertices[(index + 2) * vertexSize]));
				float frontFaceDistance = HitDistance(v0, v1, v2);
				float backFaceDistance = HitDistance(v2, v1, v0);
				currentFrontFace = std::min(frontFaceDistance > 0.0f ? frontFaceDistance : M_INFINITY, currentFrontFace);
				// A backwards face is just a regular one, with the vertices in the opposite order. This essentially checks backfaces by
				// checking reversed frontfaces
				currentBackFace = std::min(backFaceDistance > 0.0f ? backFaceDistance : M_INFINITY, currentBackFace);
				index += 3;
			}

			// If the closest face is a backface, that means that the ray originates from the inside of the geometry
			// NOTE: there may be cases where both are equal, as in, no collision to either. This is prevented in the most likely case
			// (ray doesnt hit either) by this conditional
			if (currentFrontFace != M_INFINITY || currentBackFace != M_INFINITY) {
				return currentBackFace < currentFrontFace;
			}

			// It is still possible for two triangles to be equally distant from the triangle, however, this is extremely unlikely.
			// As such, it is safe to assume they are not
			return false;
		}

		// Return whether ray is inside indexed geometry.
		bool InsideGeometry(const void* vertexData, size_t vertexSize, const void* indexData, size_t indexSize, size_t indexStart, size_t indexCount) const
		{
			float currentFrontFace = M_INFINITY;
			float currentBackFace = M_INFINITY;
			const unsigned char* vertices = (const unsigned char*)vertexData;

			const unsigned* indices = ((const unsigned*)indexData) + indexStart;
			const unsigned* indicesEnd = indices + indexCount;

			while (indices < indicesEnd) {
				const Vector3& v0 = *((const Vector3*)(&vertices[indices[0] * vertexSize]));
				const Vector3& v1 = *((const Vector3*)(&vertices[indices[1] * vertexSize]));
				const Vector3& v2 = *((const Vector3*)(&vertices[indices[2] * vertexSize]));
				float frontFaceDistance = HitDistance(v0, v1, v2);
				float backFaceDistance = HitDistance(v2, v1, v0);
				currentFrontFace = std::min(frontFaceDistance > 0.0f ? frontFaceDistance : M_INFINITY, currentFrontFace);
				// A backwards face is just a regular one, with the vertices in the opposite order.
				// This essentially checks backfaces by checking reversed frontfaces
				currentBackFace = std::min(backFaceDistance > 0.0f ? backFaceDistance : M_INFINITY, currentBackFace);
				indices += 3;
			}

			// If the closest face is a backface, that means that the ray originates from the inside of the geometry
			// NOTE: there may be cases where both are equal, as in, no collision to either. This is prevented in the most likely case
			// (ray doesnt hit either) by this conditional
			if (currentFrontFace != M_INFINITY || currentBackFace != M_INFINITY) {
				return currentBackFace < currentFrontFace;
			}

			// It is still possible for two triangles to be equally distant from the triangle, however, this is extremely unlikely.
			// As such, it is safe to assume they are not
			return false;
		}

		// Return transformed by a 3x4 matrix.
		// This may result in a non-normalized direction.
		Ray Transformed(const Matrix3x4& transform) const
		{
			Ray ret;
			ret.origin = transform * origin;
			ret.direction = transform * Vector4(direction, 0.0f);
			return ret;
		}

	public:
		// Ray origin.
		Vector3 origin;
		// Ray direction.
		Vector3 direction;
	};
}
