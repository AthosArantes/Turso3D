#include <Turso3D/Renderer/Light.h>
#include <Turso3D/Core/Allocator.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Math/Polyhedron.h>
#include <Turso3D/Math/Ray.h>
#include <Turso3D/Renderer/Camera.h>
#include <Turso3D/Renderer/DebugRenderer.h>
#include <Turso3D/Renderer/Octree.h>
#include <Turso3D/Renderer/Renderer.h>

namespace
{
	using namespace Turso3D;

	static const Quaternion PointLightFaceRotations[] =
	{
		Quaternion(0.0f, 90.0f, 0.0f),
		Quaternion(0.0f, -90.0f, 0.0f),
		Quaternion(-90.0f, 0.0f, 0.0f),
		Quaternion(90.0f, 0.0f, 0.0f),
		Quaternion(0.0f, 0.0f, 0.0f),
		Quaternion(0.0f, 180.0f, 0.0f)
	};

	static Allocator<LightDrawable> drawableAllocator;
}

namespace Turso3D
{
	LightDrawable::LightDrawable() :
		lightType(LIGHT_POINT),
		color(Color::WHITE()),
		range(10.0f),
		fov(30.0f),
		fadeStart(0.9f),
		shadowMapSize(512),
		shadowFadeStart(0.9f),
		shadowCascadeSplit(0.25f),
		shadowMaxDistance(250.0f),
		shadowMaxStrength(0.0f),
		shadowQuantize(0.5f),
		shadowMinView(10.0f),
		depthBias(2.0f),
		slopeScaleBias(1.5f),
		shadowMap(nullptr),
		shadowViewMask(1u),
		autoFocus(false)
	{
		SetFlag(Drawable::FLAG_LIGHT, true);
	}

	void LightDrawable::OnWorldBoundingBoxUpdate() const
	{
		switch (lightType) {
			case LIGHT_DIRECTIONAL:
				// Directional light always sets humongous bounding box not affected by transform
				worldBoundingBox.Define(-M_MAX_FLOAT, M_MAX_FLOAT);
				break;

			case LIGHT_POINT:
			{
				const Vector3& center = WorldPosition();
				Vector3 edge(range, range, range);
				worldBoundingBox.Define(center - edge, center + edge);
				break;
			}
			case LIGHT_SPOT:
				worldBoundingBox.Define(WorldFrustum());
				break;
		}
	}

	bool LightDrawable::OnPrepareRender(unsigned short frameNumber, Camera* camera)
	{
		switch (lightType) {
			case LIGHT_DIRECTIONAL:
				distance = 0.0f;
				break;

			case LIGHT_SPOT:
				distance = camera->Distance(WorldPosition() + 0.5f * range * WorldDirection());
				break;

			case LIGHT_POINT:
				distance = camera->Distance(WorldPosition());
				break;
		}

		if (maxDistance > 0.0f && distance > maxDistance) {
			return false;
		}

		// If there was a discontinuity in rendering the light, assume cached shadow map content lost
		if (!WasInView(frameNumber)) {
			SetShadowMap(nullptr);
		}

		lastFrameNumber = frameNumber;
		return true;
	}

	void LightDrawable::OnRaycast(std::vector<RaycastResult>& dest, const Ray& ray, float maxDistance_)
	{
		if (lightType == LIGHT_SPOT) {
			float hitDistance = ray.HitDistance(WorldFrustum());
			if (hitDistance <= maxDistance_) {
				RaycastResult res;
				res.position = ray.origin + hitDistance * ray.direction;
				res.normal = -ray.direction;
				res.distance = hitDistance;
				res.drawable = this;
				res.subObject = 0;
				dest.push_back(res);
			}
		} else if (lightType == LIGHT_POINT) {
			float hitDistance = ray.HitDistance(WorldSphere());
			if (hitDistance <= maxDistance_) {
				RaycastResult res;
				res.position = ray.origin + hitDistance * ray.direction;
				res.normal = -ray.direction;
				res.distance = hitDistance;
				res.drawable = this;
				res.subObject = 0;
				dest.push_back(res);
			}
		}
	}

	void LightDrawable::OnRenderDebug(DebugRenderer* debug)
	{
		if (lightType == LIGHT_SPOT) {
			debug->AddFrustum(WorldFrustum(), color, false);
		} else if (lightType == LIGHT_POINT) {
			debug->AddSphere(WorldSphere(), color, false);
		}
	}

	IntVector2 LightDrawable::TotalShadowMapSize() const
	{
		if (lightType == LIGHT_DIRECTIONAL) {
			return IntVector2 {shadowMapSize * 2, shadowMapSize};
		} else if (lightType == LIGHT_POINT) {
			return IntVector2 {shadowMapSize * 3, shadowMapSize * 2};
		} else {
			return IntVector2 {shadowMapSize, shadowMapSize};
		}
	}

	Color LightDrawable::EffectiveColor() const
	{
		if (maxDistance > 0.0f) {
			float scaledDistance = distance / maxDistance;
			if (scaledDistance >= shadowFadeStart) {
				return color.Lerp(Color::BLACK(), (scaledDistance - fadeStart) / (1.0f - fadeStart));
			}
		}
		return color;
	}

	float LightDrawable::ShadowStrength() const
	{
		if (!TestFlag(Drawable::FLAG_CAST_SHADOWS)) {
			return 1.0f;
		}

		if (lightType != LIGHT_DIRECTIONAL && shadowMaxDistance > 0.0f) {
			float scaledDistance = distance / shadowMaxDistance;
			if (scaledDistance >= shadowFadeStart) {
				return Lerp(shadowMaxStrength, 1.0f, (scaledDistance - shadowFadeStart) / (1.0f - shadowFadeStart));
			}
		}

		return shadowMaxStrength;
	}

	Vector2 LightDrawable::ShadowCascadeSplits() const
	{
		return Vector2 {shadowCascadeSplit * shadowMaxDistance, shadowMaxDistance};
	}

	size_t LightDrawable::NumShadowViews() const
	{
		if (!TestFlag(Drawable::FLAG_CAST_SHADOWS)) {
			return 0;
		} else if (lightType == LIGHT_DIRECTIONAL) {
			return 2;
		} else if (lightType == LIGHT_POINT) {
			return 6;
		} else {
			return 1;
		}
	}

	Frustum LightDrawable::WorldFrustum() const
	{
		const Matrix3x4& transform = WorldTransform();
		Matrix3x4 frustumTransform(transform.Translation(), transform.Rotation(), 1.0f);
		Frustum ret;
		ret.Define(fov, 1.0f, 1.0f, 0.0f, range, frustumTransform);
		return ret;
	}

	Sphere LightDrawable::WorldSphere() const
	{
		return Sphere(WorldPosition(), range);
	}

	void LightDrawable::SetShadowMap(Texture* shadowMap_, const IntRect& shadowRect_)
	{
		if (!shadowMap) {
			shadowViews.clear();
		}
		shadowMap = shadowMap_;
		shadowRect = shadowRect_;
	}

	void LightDrawable::InitShadowViews()
	{
		shadowViews.resize(NumShadowViews());

		for (size_t i = 0; i < shadowViews.size(); ++i) {
			ShadowView& view = shadowViews[i];
			view.light = this;

			if (!view.shadowCamera) {
				view.shadowCamera = std::make_unique<Camera>();
				// OpenGL render-to-texture needs vertical flip
				view.shadowCamera->SetFlipVertical(true);
			}
		}

		// Calculate shadow mapping constants common to all lights
		const IntVector2& sz = shadowMap->Size2D();
		shadowParameters = Vector4 {0.5f / static_cast<float>(sz.x), 0.5f / static_cast<float>(sz.y), ShadowStrength(), 0.0f};
	}

	bool LightDrawable::SetupShadowView(size_t viewIndex, Camera* mainCamera, const BoundingBox* geometryBounds)
	{
		ShadowView& view = shadowViews[viewIndex];
		Camera* shadowCamera = view.shadowCamera.get();
		int actualShadowMapSize = ActualShadowMapSize();

		switch (lightType) {
			case LIGHT_DIRECTIONAL:
			{
				IntVector2 topLeft {shadowRect.left, shadowRect.top};
				if (viewIndex & 1) {
					topLeft.x += actualShadowMapSize;
				}
				view.viewport = IntRect {topLeft.x, topLeft.y, topLeft.x + actualShadowMapSize, topLeft.y + actualShadowMapSize};
				Vector2 cascadeSplits = ShadowCascadeSplits();

				view.splitMinZ = std::max(mainCamera->NearClip(), (viewIndex == 0) ? 0.0f : cascadeSplits.x);
				view.splitMaxZ = std::min(mainCamera->FarClip(), (viewIndex == 0) ? cascadeSplits.x : cascadeSplits.y);
				float extrusionDistance = mainCamera->FarClip();

				// Calculate initial position & rotation
				shadowCamera->SetTransform(mainCamera->WorldPosition() - extrusionDistance * WorldDirection(), WorldRotation());

				// Calculate main camera shadowed frustum in light's view space.
				// Then convert to polyhedron and clip with visible geometry, and transform to shadow camera's space
				Frustum splitFrustum = mainCamera->WorldSplitFrustum(view.splitMinZ, view.splitMaxZ);
				BoundingBox shadowBox;

				if (geometryBounds) {
					// If geometry bounds given, but is not defined (no geometry), skip rendering the view
					if (!geometryBounds->IsDefined()) {
						return false;
					}

					Polyhedron frustumVolume(splitFrustum);
					frustumVolume.Clip(*geometryBounds);

					// If volume became empty, skip rendering the view
					if (frustumVolume.IsEmpty()) {
						return false;
					}

					// Fit the clipped volume inside a bounding box
					frustumVolume.Transform(shadowCamera->ViewMatrix());
					shadowBox.Define(frustumVolume);
				} else {
					// If no bounds available, define the shadow space frustum bounding box directly without clipping
					shadowBox.Define(splitFrustum.Transformed(shadowCamera->ViewMatrix()));
				}

				// If shadow camera is far away from the frustum, can bring it closer for better depth precision
				// TODO: The minimum distance is somewhat arbitrary
				float minDistance = mainCamera->FarClip() * 0.25f;
				if (shadowBox.min.z > minDistance) {
					float move = shadowBox.min.z - minDistance;
					shadowCamera->Translate(Vector3(0.0f, 0.f, move));
					shadowBox.min.z -= move,
						shadowBox.max.z -= move;
				}

				shadowCamera->SetOrthographic(true);
				shadowCamera->SetFarClip(shadowBox.max.z);

				Vector3 center = shadowBox.Center();
				Vector3 size = shadowBox.Size();

				size.x = ceilf(sqrtf(size.x / shadowQuantize));
				size.y = ceilf(sqrtf(size.y / shadowQuantize));
				size.x = std::max(size.x * size.x * shadowQuantize, shadowMinView);
				size.y = std::max(size.y * size.y * shadowQuantize, shadowMinView);

				shadowCamera->SetOrthoSize(Vector2 {size.x, size.y});
				shadowCamera->SetZoom(1.0f);

				// Center shadow camera to the view space bounding box
				Quaternion rot {shadowCamera->WorldRotation()};
				Vector3 adjust {center.x, center.y, 0.0f};
				shadowCamera->Translate(rot * adjust, TS_WORLD);

				// Snap to whole texels
				{
					Vector3 viewPos {rot.Inverse() * shadowCamera->WorldPosition()};
					float invSize = 4.0f / actualShadowMapSize;
					Vector2 texelSize {size.x * invSize, size.y * invSize};
					Vector3 snap {-fmodf(viewPos.x, texelSize.x), -fmodf(viewPos.y, texelSize.y), 0.0f};
					shadowCamera->Translate(rot * snap, TS_WORLD);
				}
			}
			break;

			case LIGHT_POINT:
			{
				IntVector2 topLeft {shadowRect.left, shadowRect.top};
				if (viewIndex & 1) {
					topLeft.y += actualShadowMapSize;
				}
				topLeft.x += ((unsigned)viewIndex >> 1) * actualShadowMapSize;
				view.viewport = IntRect {topLeft.x, topLeft.y, topLeft.x + actualShadowMapSize, topLeft.y + actualShadowMapSize};

				shadowCamera->SetTransform(WorldPosition(), PointLightFaceRotations[viewIndex]);
				shadowCamera->SetFov(90.0f);
				shadowCamera->SetZoom((float)(actualShadowMapSize - 4) / (float)actualShadowMapSize);
				shadowCamera->SetFarClip(Range());
				shadowCamera->SetNearClip(Range() * 0.01f);
				shadowCamera->SetOrthographic(false);
				shadowCamera->SetAspectRatio(1.0f);
			}
			break;

			case LIGHT_SPOT:
				view.viewport = shadowRect;
				shadowCamera->SetTransform(WorldPosition(), WorldRotation());
				shadowCamera->SetFov(fov);
				shadowCamera->SetZoom(1.0f);
				shadowCamera->SetFarClip(Range());
				shadowCamera->SetNearClip(Range() * 0.01f);
				shadowCamera->SetOrthographic(false);
				shadowCamera->SetAspectRatio(1.0f);
				break;
		}

		view.shadowFrustum = shadowCamera->WorldFrustum();

		// Setup shadow matrices now as camera positions have been finalized
		if (lightType != LIGHT_POINT) {
			const IntVector2& sz = shadowMap->Size2D();
			Vector2 textureSize {static_cast<float>(sz.x), static_cast<float>(sz.y)};

			Vector3 viewOffset {
				static_cast<float>(view.viewport.left) / textureSize.x,
				static_cast<float>(view.viewport.top) / textureSize.y,
				0.0f
			};
			Vector3 viewScale {
				0.5f * static_cast<float>(view.viewport.Width()) / textureSize.x,
				0.5f * static_cast<float>(view.viewport.Height()) / textureSize.y,
				1.0f
			};

			viewOffset.x += viewScale.x;
			viewOffset.y += viewScale.y;

			// OpenGL has different depth range
			viewOffset.z = 0.5f;
			viewScale.z = 0.5f;

			Matrix4 texAdjust {Matrix4::IDENTITY()};
			texAdjust.SetTranslation(viewOffset);
			texAdjust.SetScale(viewScale);

			view.shadowMatrix = texAdjust * shadowCamera->ProjectionMatrix() * shadowCamera->ViewMatrix();
		} else {
			if (!viewIndex) {
				const IntVector2& sz = shadowMap->Size2D();
				Vector2 textureSize {static_cast<float>(sz.x), static_cast<float>(sz.y)};

				Vector3 worldPosition = WorldPosition();
				float nearClip = Range() * 0.01f;
				float farClip = Range();
				float q = farClip / (farClip - nearClip);
				float r = -q * nearClip;

				Matrix4& shadowMatrix = view.shadowMatrix;
				shadowMatrix.m00 = actualShadowMapSize / textureSize.x;
				shadowMatrix.m01 = actualShadowMapSize / textureSize.y;
				shadowMatrix.m02 = (float)shadowRect.left / textureSize.x;
				shadowMatrix.m03 = (float)shadowRect.top / textureSize.y;
				shadowMatrix.m10 = shadowCamera->Zoom();
				shadowMatrix.m11 = q;
				shadowMatrix.m12 = r;
				// Put position here for invalidating dynamic light shadowmaps when the light moves
				shadowMatrix.m20 = worldPosition.x;
				shadowMatrix.m21 = worldPosition.y;
				shadowMatrix.m22 = worldPosition.z;
			} else {
				// Shadow matrix for the rest of the cubemap sides is copied from the first
				view.shadowMatrix = shadowViews[0].shadowMatrix;
			}
		}

		return true;
	}

	// ==========================================================================================
	Light::Light()
	{
		drawable = drawableAllocator.Allocate();
		drawable->SetOwner(this);
	}

	Light::~Light()
	{
		RemoveFromOctree();
		drawableAllocator.Free(static_cast<LightDrawable*>(drawable));
		drawable = nullptr;
	}

	void Light::SetLightType(LightType type)
	{
		LightDrawable* drawable = GetDrawable();
		if (type != drawable->lightType) {
			drawable->lightType = type;
			OnBoundingBoxChanged();
		}
	}

	void Light::SetColor(const Color& color)
	{
		LightDrawable* drawable = GetDrawable();
		drawable->color = color;
	}

	void Light::SetRange(float range_)
	{
		LightDrawable* drawable = GetDrawable();
		range_ = std::max(range_, 0.0f);
		if (range_ != drawable->range) {
			drawable->range = range_;
			OnBoundingBoxChanged();
		}
	}

	void Light::SetFov(float fov_)
	{
		LightDrawable* drawable = GetDrawable();
		fov_ = Clamp(fov_, 0.0f, 180.0f);
		if (fov_ != drawable->fov) {
			drawable->fov = fov_;
			OnBoundingBoxChanged();
		}
	}

	void Light::SetFadeStart(float start)
	{
		LightDrawable* drawable = GetDrawable();
		drawable->fadeStart = Clamp(start, 0.0f, 1.0f - M_EPSILON);
	}

	void Light::SetShadowMapSize(int size)
	{
		LightDrawable* drawable = GetDrawable();
		drawable->shadowMapSize = NextPowerOfTwo(std::max(1, size));
	}

	void Light::SetShadowFadeStart(float start)
	{
		LightDrawable* drawable = GetDrawable();
		drawable->shadowFadeStart = Clamp(start, 0.0f, 1.0f - M_EPSILON);
	}

	void Light::SetShadowCascadeSplit(float split)
	{
		LightDrawable* drawable = GetDrawable();
		drawable->shadowCascadeSplit = Clamp(split, M_EPSILON, 1.0f - M_EPSILON);
	}

	void Light::SetShadowMaxDistance(float distance_)
	{
		LightDrawable* drawable = GetDrawable();
		drawable->shadowMaxDistance = std::max(distance_, 0.0f);
	}

	void Light::SetShadowMaxStrength(float strength)
	{
		LightDrawable* drawable = GetDrawable();
		drawable->shadowMaxStrength = Clamp(strength, 0.0f, 1.f);
	}

	void Light::SetShadowQuantize(float quantize)
	{
		LightDrawable* drawable = GetDrawable();
		drawable->shadowQuantize = std::max(quantize, M_EPSILON);
	}

	void Light::SetShadowMinView(float minView)
	{
		LightDrawable* drawable = GetDrawable();
		drawable->shadowMinView = std::max(minView, M_EPSILON);
	}

	void Light::SetDepthBias(float bias)
	{
		LightDrawable* drawable = GetDrawable();
		drawable->depthBias = std::max(bias, 0.0f);
	}

	void Light::SetSlopeScaleBias(float bias)
	{
		LightDrawable* drawable = GetDrawable();
		drawable->slopeScaleBias = std::max(bias, 0.0f);
	}

	void Light::SetShadowViewMask(unsigned mask)
	{
		LightDrawable* drawable = GetDrawable();
		drawable->shadowViewMask = mask;
	}

	void Light::SetAutoFocus(bool autoFocus)
	{
		LightDrawable* drawable = GetDrawable();
		drawable->autoFocus = autoFocus;
	}
}
