#include <Turso3D/Renderer/Camera.h>
#include <Turso3D/Math/Matrix3x4.h>

namespace
{
	using namespace Turso3D;

	static const Matrix4 FlipMatrix(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
}

namespace Turso3D
{
	Camera::Camera() :
		viewMatrix(Matrix3x4::IDENTITY()),
		viewMatrixDirty(false),
		orthographic(false),
		flipVertical(false),
		nearClip(0.1f),
		farClip(1000.0f),
		fov(45.0f),
		orthoSize(20.0f),
		aspectRatio(1.0f),
		zoom(1.0f),
		lodBias(1.0f),
		viewMask(1u),
		reflectionPlane(Plane::UP()),
		clipPlane(Plane::UP()),
		useReflection(false),
		useClipping(false)
	{
		reflectionMatrix = reflectionPlane.ReflectionMatrix();
	}

	float Camera::NearClip() const
	{
		// Orthographic camera has always near clip at 0 to avoid trouble with shader depth parameters,
		// and unlike in perspective mode there should be no depth buffer precision issue
		return orthographic ? 0.0f : nearClip;
	}

	Frustum Camera::WorldFrustum() const
	{
		Frustum ret;

		if (!orthographic) {
			ret.Define(fov, aspectRatio, zoom, NearClip(), farClip, EffectiveWorldTransform());
		} else {
			ret.DefineOrtho(orthoSize, aspectRatio, zoom, NearClip(), farClip, EffectiveWorldTransform());
		}

		return ret;
	}

	Frustum Camera::WorldSplitFrustum(float nearClip_, float farClip_) const
	{
		Frustum ret;

		nearClip_ = std::max(nearClip_, NearClip());
		farClip_ = std::min(farClip_, farClip);

		if (!orthographic) {
			ret.Define(fov, aspectRatio, zoom, nearClip_, farClip_, EffectiveWorldTransform());
		} else {
			ret.DefineOrtho(orthoSize, aspectRatio, zoom, nearClip_, farClip_, EffectiveWorldTransform());
		}

		return ret;
	}

	Frustum Camera::ViewSpaceFrustum() const
	{
		Frustum ret;

		if (!orthographic) {
			ret.Define(fov, aspectRatio, zoom, NearClip(), farClip);
		} else {
			ret.DefineOrtho(orthoSize, aspectRatio, zoom, NearClip(), farClip);
		}

		return ret;
	}

	Frustum Camera::ViewSpaceSplitFrustum(float nearClip_, float farClip_) const
	{
		Frustum ret;

		nearClip_ = std::max(nearClip_, NearClip());
		farClip_ = std::min(farClip_, farClip);
		if (farClip_ < nearClip_) {
			farClip_ = nearClip_;
		}

		if (!orthographic) {
			ret.Define(fov, aspectRatio, zoom, nearClip_, farClip_);
		} else {
			ret.DefineOrtho(orthoSize, aspectRatio, zoom, nearClip_, farClip_);
		}

		return ret;
	}

	Matrix4 Camera::ProjectionMatrix(bool apiSpecific) const
	{
		Matrix4 ret(Matrix4::ZERO());

		bool openGLFormat = apiSpecific;

		if (!orthographic) {
			float h = (1.0f / tanf(fov * M_DEGTORAD * 0.5f)) * zoom;
			float w = h / aspectRatio;
			float q = farClip / (farClip - nearClip);
			float r = -q * nearClip;

			ret.m00 = w;
			ret.m02 = 0.0f;
			ret.m11 = h;
			ret.m12 = 0.0f;
			ret.m22 = q;
			ret.m23 = r;
			ret.m32 = 1.0f;
		} else {
			// Disregard near clip, because it does not affect depth precision as with perspective projection
			float h = (1.0f / (orthoSize * 0.5f)) * zoom;
			float w = h / aspectRatio;
			float q = 1.0f / farClip;
			float r = 0.0f;

			ret.m00 = w;
			ret.m03 = 0.0f;
			ret.m11 = h;
			ret.m13 = 0.0f;
			ret.m22 = q;
			ret.m23 = r;
			ret.m33 = 1.0f;
		}

		if (flipVertical) {
			ret = FlipMatrix * ret;
		}

		if (openGLFormat) {
			ret.m20 = 2.0f * ret.m20 - ret.m30;
			ret.m21 = 2.0f * ret.m21 - ret.m31;
			ret.m22 = 2.0f * ret.m22 - ret.m32;
			ret.m23 = 2.0f * ret.m23 - ret.m33;
		}

		return ret;
	}

	void Camera::FrustumSize(Vector3& near, Vector3& far) const
	{
		near.z = NearClip();
		far.z = farClip;

		if (!orthographic) {
			float halfViewSize = tanf(fov * M_DEGTORAD * 0.5f) / zoom;
			near.y = near.z * halfViewSize;
			near.x = near.y * aspectRatio;
			far.y = far.z * halfViewSize;
			far.x = far.y * aspectRatio;
		} else {
			float halfViewSize = orthoSize * 0.5f / zoom;
			near.y = far.y = halfViewSize;
			near.x = far.x = near.y * aspectRatio;
		}

		if (flipVertical) {
			near.y = -near.y;
			far.y = -far.y;
		}
	}

	float Camera::HalfViewSize() const
	{
		if (!orthographic) {
			return tanf(fov * M_DEGTORAD * 0.5f) / zoom;
		} else {
			return orthoSize * 0.5f / zoom;
		}
	}

	Ray Camera::ScreenRay(float x, float y) const
	{
		Ray ret;

		// If projection is invalid, just return a ray pointing forward
		if (!IsProjectionValid()) {
			ret.origin = WorldPosition();
			ret.direction = WorldDirection();
			return ret;
		}

		Matrix4 viewProjInverse = (ProjectionMatrix(false) * ViewMatrix()).Inverse();

		// The parameters range from 0.0 to 1.0. Expand to normalized device coordinates (-1.0 to 1.0) & flip Y axis
		x = 2.0f * x - 1.0f;
		y = 1.0f - 2.0f * y;
		Vector3 near(x, y, 0.0f);
		Vector3 far(x, y, 1.0f);

		ret.origin = viewProjInverse * near;
		ret.direction = ((viewProjInverse * far) - ret.origin).Normalized();
		return ret;
	}

	Vector2 Camera::WorldToScreenPoint(const Vector3& worldPos) const
	{
		Vector3 eyeSpacePos = ViewMatrix() * worldPos;
		Vector2 ret;

		if (eyeSpacePos.z > 0.0f) {
			Vector3 screenSpacePos = ProjectionMatrix(false) * eyeSpacePos;
			ret.x = screenSpacePos.x;
			ret.y = screenSpacePos.y;
		} else {
			ret.x = (-eyeSpacePos.x > 0.0f) ? -1.0f : 1.0f;
			ret.y = (-eyeSpacePos.y > 0.0f) ? -1.0f : 1.0f;
		}

		ret.x = (ret.x * 0.5f) + 0.5f;
		ret.y = 1.0f - ((ret.y * 0.5f) + 0.5f);
		return ret;
	}

	Vector3 Camera::ScreenToWorldPoint(const Vector3& screenPos) const
	{
		Ray ray = ScreenRay(screenPos.x, screenPos.y);
		return ray.origin + ray.direction * screenPos.z;
	}

	Quaternion Camera::FaceCameraRotation(const Vector3& position_, const Quaternion& rotation_, FaceCameraMode mode)
	{
		switch (mode) {
			default:
				return rotation_;

			case FC_ROTATE_XYZ:
				return WorldRotation();

			case FC_ROTATE_Y:
			{
				Vector3 euler = rotation_.EulerAngles();
				euler.y = WorldRotation().EulerAngles().y;
				return Quaternion(euler.x, euler.y, euler.z);
			}

			case FC_LOOKAT_XYZ:
			{
				Quaternion lookAt;
				lookAt.FromLookRotation(position_ - WorldPosition());
				return lookAt;
			}

			case FC_LOOKAT_Y:
			{
				// Make the Y-only lookat happen on an XZ plane to make sure there are no unwanted transitions or singularities
				Vector3 lookAtVec(position_ - WorldPosition());
				lookAtVec.y = 0.0f;

				Quaternion lookAt;
				lookAt.FromLookRotation(lookAtVec);

				Vector3 euler = rotation_.EulerAngles();
				euler.y = lookAt.EulerAngles().y;
				return Quaternion(euler.x, euler.y, euler.z);
			}
		}
	}

	Matrix3x4 Camera::EffectiveWorldTransform() const
	{
		Matrix3x4 transform(WorldPosition(), WorldRotation(), 1.0f);
		return useReflection ? reflectionMatrix * transform : transform;
	}

	bool Camera::IsProjectionValid() const
	{
		return farClip > NearClip();
	}

	void Camera::OnTransformChanged()
	{
		SpatialNode::OnTransformChanged();
		viewMatrixDirty = true;
	}
}
