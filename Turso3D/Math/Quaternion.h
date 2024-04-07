#pragma once

#include <Turso3D/Math/Matrix3.h>

namespace Turso3D
{
	// Rotation represented as a four-dimensional normalized vector.
	class Quaternion
	{
	public:
		// Construct undefined.
		Quaternion()
		{
		}

		// Copy-construct.
		Quaternion(const Quaternion& quat) :
			w(quat.w),
			x(quat.x),
			y(quat.y),
			z(quat.z)
		{
		}

		// Construct from values.
		Quaternion(float w, float x, float y, float z) :
			w(w),
			x(x),
			y(y),
			z(z)
		{
		}

		// Construct from a float array.
		Quaternion(const float* data) :
			w(data[0]),
			x(data[1]),
			y(data[2]),
			z(data[3])
		{
		}

		// Construct from an angle (in degrees) and axis.
		Quaternion(float angle, const Vector3& axis)
		{
			FromAngleAxis(angle, axis);
		}

		// Construct from Euler angles (in degrees.)
		Quaternion(float x, float y, float z)
		{
			FromEulerAngles(x, y, z);
		}

		// Construct from the rotation difference between two direction vectors.
		Quaternion(const Vector3& start, const Vector3& end)
		{
			FromRotationTo(start, end);
		}

		// Construct from orthonormal axes.
		Quaternion(const Vector3& xAxis, const Vector3& yAxis, const Vector3& zAxis)
		{
			FromAxes(xAxis, yAxis, zAxis);
		}

		// Construct from a rotation matrix.
		Quaternion(const Matrix3& matrix)
		{
			FromRotationMatrix(matrix);
		}

		// Assign from another quaternion.
		Quaternion& operator = (const Quaternion& rhs)
		{
			w = rhs.w;
			x = rhs.x;
			y = rhs.y;
			z = rhs.z;
			return *this;
		}

		// Add-assign a quaternion.
		Quaternion& operator += (const Quaternion& rhs)
		{
			w += rhs.w;
			x += rhs.x;
			y += rhs.y;
			z += rhs.z;
			return *this;
		}

		// Multiply-assign a scalar.
		Quaternion& operator *= (float rhs)
		{
			w *= rhs;
			x *= rhs;
			y *= rhs;
			z *= rhs;
			return *this;
		}

		// Test for equality with another quaternion without epsilon.
		bool operator == (const Quaternion& rhs) const
		{
			return w == rhs.w && x == rhs.x && y == rhs.y && z == rhs.z;
		}
		// Test for inequality with another quaternion without epsilon.
		bool operator != (const Quaternion& rhs) const
		{
			return !(*this == rhs);
		}
		// Multiply with a scalar.
		Quaternion operator * (float rhs) const
		{
			return Quaternion(w * rhs, x * rhs, y * rhs, z * rhs);
		}
		// Return negation.
		Quaternion operator - () const
		{
			return Quaternion(-w, -x, -y, -z);
		}
		// Add a quaternion.
		Quaternion operator + (const Quaternion& rhs) const
		{
			return Quaternion(w + rhs.w, x + rhs.x, y + rhs.y, z + rhs.z);
		}
		// Subtract a quaternion.
		Quaternion operator - (const Quaternion& rhs) const
		{
			return Quaternion(w - rhs.w, x - rhs.x, y - rhs.y, z - rhs.z);
		}

		// Multiply a quaternion.
		Quaternion operator * (const Quaternion& rhs) const
		{
			return Quaternion(
				w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z,
				w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
				w * rhs.y + y * rhs.w + z * rhs.x - x * rhs.z,
				w * rhs.z + z * rhs.w + x * rhs.y - y * rhs.x
			);
		}

		// Multiply a Vector3.
		Vector3 operator * (const Vector3& rhs) const
		{
			Vector3 qVec(x, y, z);
			Vector3 cross1(qVec.CrossProduct(rhs));
			Vector3 cross2(qVec.CrossProduct(cross1));
			return rhs + 2.0f * (cross1 * w + cross2);
		}

		// Define from an angle (in degrees) and axis.
		void FromAngleAxis(float angle, const Vector3& axis)
		{
			Vector3 normAxis = axis.Normalized();
			angle *= M_DEGTORAD_2;
			float sinAngle = sinf(angle);
			float cosAngle = cosf(angle);

			w = cosAngle;
			x = normAxis.x * sinAngle;
			y = normAxis.y * sinAngle;
			z = normAxis.z * sinAngle;
		}

		// Define from Euler angles (in degrees.)
		void FromEulerAngles(float x, float y, float z)
		{
			// Order of rotations: Z first, then X, then Y (mimics typical FPS camera with gimbal lock at top/bottom)
			x *= M_DEGTORAD_2;
			y *= M_DEGTORAD_2;
			z *= M_DEGTORAD_2;
			float sinX = sinf(x);
			float cosX = cosf(x);
			float sinY = sinf(y);
			float cosY = cosf(y);
			float sinZ = sinf(z);
			float cosZ = cosf(z);

			this->w = cosY * cosX * cosZ + sinY * sinX * sinZ;
			this->x = cosY * sinX * cosZ + sinY * cosX * sinZ;
			this->y = sinY * cosX * cosZ - cosY * sinX * sinZ;
			this->z = cosY * cosX * sinZ - sinY * sinX * cosZ;
		}

		// Define from the rotation difference between two direction vectors.
		void FromRotationTo(const Vector3& start, const Vector3& end)
		{
			Vector3 normStart = start.Normalized();
			Vector3 normEnd = end.Normalized();
			float d = normStart.DotProduct(normEnd);

			if (d > -1.0f + M_EPSILON) {
				Vector3 c = normStart.CrossProduct(normEnd);
				float s = sqrtf((1.0f + d) * 2.0f);
				float invS = 1.0f / s;

				x = c.x * invS;
				y = c.y * invS;
				z = c.z * invS;
				w = 0.5f * s;
			} else {
				Vector3 axis = Vector3::RIGHT().CrossProduct(normStart);
				if (axis.Length() < M_EPSILON) {
					axis = Vector3::UP().CrossProduct(normStart);
				}
				FromAngleAxis(180.f, axis);
			}
		}

		// Define from orthonormal axes.
		void FromAxes(const Vector3& xAxis, const Vector3& yAxis, const Vector3& zAxis)
		{
			Matrix3 matrix(
				xAxis.x, yAxis.x, zAxis.x,
				xAxis.y, yAxis.y, zAxis.y,
				xAxis.z, yAxis.z, zAxis.z
			);
			FromRotationMatrix(matrix);
		}

		// Define from a rotation matrix.
		void FromRotationMatrix(const Matrix3& matrix)
		{
			float t = matrix.m00 + matrix.m11 + matrix.m22;

			if (t > 0.0f) {
				float invS = 0.5f / sqrtf(1.0f + t);

				x = (matrix.m21 - matrix.m12) * invS;
				y = (matrix.m02 - matrix.m20) * invS;
				z = (matrix.m10 - matrix.m01) * invS;
				w = 0.25f / invS;
			} else {
				if (matrix.m00 > matrix.m11 && matrix.m00 > matrix.m22) {
					float invS = 0.5f / sqrtf(1.0f + matrix.m00 - matrix.m11 - matrix.m22);

					x = 0.25f / invS;
					y = (matrix.m01 + matrix.m10) * invS;
					z = (matrix.m20 + matrix.m02) * invS;
					w = (matrix.m21 - matrix.m12) * invS;
				} else if (matrix.m11 > matrix.m22) {
					float invS = 0.5f / sqrtf(1.0f + matrix.m11 - matrix.m00 - matrix.m22);

					x = (matrix.m01 + matrix.m10) * invS;
					y = 0.25f / invS;
					z = (matrix.m12 + matrix.m21) * invS;
					w = (matrix.m02 - matrix.m20) * invS;
				} else {
					float invS = 0.5f / sqrtf(1.0f + matrix.m22 - matrix.m00 - matrix.m11);

					x = (matrix.m02 + matrix.m20) * invS;
					y = (matrix.m12 + matrix.m21) * invS;
					z = 0.25f / invS;
					w = (matrix.m10 - matrix.m01) * invS;
				}
			}
		}

		// Define from a direction to look in and an up direction.
		// Return true on success, or false if would result in a NaN, in which case the current value remains.
		bool FromLookRotation(const Vector3& direction, const Vector3& upDirection = Vector3::UP())
		{
			Quaternion ret;
			Vector3 forward = direction.Normalized();

			Vector3 v = forward.CrossProduct(upDirection);
			// If direction & upDirection are parallel and crossproduct becomes zero, use FromRotationTo() fallback
			if (v.LengthSquared() >= M_EPSILON) {
				v.Normalize();
				Vector3 up = v.CrossProduct(forward);
				Vector3 right = up.CrossProduct(forward);
				ret.FromAxes(right, up, forward);
			} else {
				ret.FromRotationTo(Vector3::FORWARD(), forward);
			}

			if (!ret.IsNaN()) {
				(*this) = ret;
				return true;
			} else {
				return false;
			}
		}

		// Normalize to unit length.
		void Normalize()
		{
			float lenSquared = LengthSquared();
			if (!EpsilonEquals(lenSquared, 1.0f) && lenSquared > 0.0f) {
				float invLen = 1.0f / sqrtf(lenSquared);
				w *= invLen;
				x *= invLen;
				y *= invLen;
				z *= invLen;
			}
		}

		// Return normalized to unit length.
		Quaternion Normalized() const
		{
			float lenSquared = LengthSquared();
			if (!EpsilonEquals(lenSquared, 1.0f) && lenSquared > 0.0f) {
				float invLen = 1.0f / sqrtf(lenSquared);
				return *this * invLen;
			}
			return *this;
		}

		// Return inverse.
		Quaternion Inverse() const
		{
			float lenSquared = LengthSquared();
			if (lenSquared == 1.0f) {
				return Conjugate();
			} else if (lenSquared >= M_EPSILON) {
				return Conjugate() * (1.0f / lenSquared);
			}
			return IDENTITY();
		}

		// Return squared length.
		float LengthSquared() const
		{
			return w * w + x * x + y * y + z * z;
		}
		// Calculate dot product.
		float DotProduct(const Quaternion& rhs) const
		{
			return w * rhs.w + x * rhs.x + y * rhs.y + z * rhs.z;
		}
		// Test for equality with another quaternion with epsilon.
		bool Equals(const Quaternion& rhs, float epsilon = M_EPSILON) const
		{
			return EpsilonEquals(w, rhs.w, epsilon) && EpsilonEquals(x, rhs.x, epsilon) && EpsilonEquals(y, rhs.y, epsilon) && EpsilonEquals(z, rhs.z, epsilon);
		}
		// Return whether is NaN.
		bool IsNaN() const
		{
			return (w != w) || (x != x) || (y != y) || (z != z);
		}
		// Return conjugate.
		Quaternion Conjugate() const
		{
			return Quaternion(w, -x, -y, -z);
		}

		// Return Euler angles in degrees.
		Vector3 EulerAngles() const
		{
			// Derivation from http://www.geometrictools.com/Documentation/EulerAngles.pdf
			// Order of rotations: Z first, then X, then Y
			float check = 2.0f * (-y * z + w * x);

			if (check < -0.995f) {
				return Vector3(
					-90.0f,
					0.0f,
					-atan2f(2.0f * (x * z - w * y), 1.0f - 2.0f * (y * y + z * z)) * M_RADTODEG
				);
			} else if (check > 0.995f) {
				return Vector3(
					90.0f,
					0.0f,
					atan2f(2.0f * (x * z - w * y), 1.0f - 2.0f * (y * y + z * z)) * M_RADTODEG
				);
			} else {
				return Vector3(
					asinf(check) * M_RADTODEG,
					atan2f(2.0f * (x * z + w * y), 1.0f - 2.0f * (x * x + y * y)) * M_RADTODEG,
					atan2f(2.0f * (x * y + w * z), 1.0f - 2.0f * (x * x + z * z)) * M_RADTODEG
				);
			}
		}

		// Return yaw angle in degrees.
		float YawAngle() const { return EulerAngles().y; }
		// Return pitch angle in degrees.
		float PitchAngle() const { return EulerAngles().x; }
		// Return roll angle in degrees.
		float RollAngle() const { return EulerAngles().z; }

		// Return the rotation matrix that corresponds to this quaternion.
		Matrix3 RotationMatrix() const
		{
			return Matrix3(
				1.0f - 2.0f * y * y - 2.0f * z * z,
				2.0f * x * y - 2.0f * w * z,
				2.0f * x * z + 2.0f * w * y,
				2.0f * x * y + 2.0f * w * z,
				1.0f - 2.0f * x * x - 2.0f * z * z,
				2.0f * y * z - 2.0f * w * x,
				2.0f * x * z - 2.0f * w * y,
				2.0f * y * z + 2.0f * w * x,
				1.0f - 2.0f * x * x - 2.0f * y * y
			);
		}

		// Spherical interpolation with another quaternion.
		Quaternion Slerp(Quaternion rhs, float t) const
		{
			float cosAngle = DotProduct(rhs);
			// Enable shortest path rotation
			if (cosAngle < 0.0f) {
				cosAngle = -cosAngle;
				rhs = -rhs;
			}

			float angle = acosf(cosAngle);
			float sinAngle = sinf(angle);
			float t1, t2;

			if (sinAngle > 0.001f) {
				float invSinAngle = 1.0f / sinAngle;
				t1 = sinf((1.0f - t) * angle) * invSinAngle;
				t2 = sinf(t * angle) * invSinAngle;
			} else {
				t1 = 1.0f - t;
				t2 = t;
			}

			return *this * t1 + rhs * t2;
		}

		// Normalized linear interpolation with another quaternion.
		Quaternion Nlerp(Quaternion rhs, float t, bool shortestPath = false) const
		{
			Quaternion result;
			float fCos = DotProduct(rhs);
			if (fCos < 0.0f && shortestPath) {
				result = (*this) + (((-rhs) - (*this)) * t);
			} else {
				result = (*this) + ((rhs - (*this)) * t);
			}
			result.Normalize();
			return result;
		}

		// Return float data.
		const float* Data() const
		{
			return &w;
		}

		// Identity quaternion.
		static Quaternion IDENTITY();

	public:
		// W coordinate.
		float w;
		// X coordinate.
		float x;
		// Y coordinate.
		float y;
		// Z coordinate.
		float z;
	};

	// ==========================================================================================
	inline Quaternion Quaternion::IDENTITY()
	{
		return Quaternion {1.0f, 0.0f, 0.0f, 0.0f};
	}
}
