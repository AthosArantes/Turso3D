#pragma once

#include <Turso3D/Renderer/GeometryNode.h>
#include <memory>

namespace Turso3D
{
	class Model;

	// Static model drawable.
	class StaticModelDrawable : public GeometryDrawable
	{
		friend class StaticModel;

	public:
		// Construct.
		StaticModelDrawable();

		// Recalculate the world space bounding box.
		void OnWorldBoundingBoxUpdate() const override;
		// Prepare object for rendering.
		// Reset framenumber and calculate distance from camera, and check for LOD level changes.
		// Called by Renderer in worker threads. Return false if should not render.
		bool OnPrepareRender(unsigned short frameNumber, Camera* camera) override;
		// Perform ray test on self and add possible hit to the result vector.
		void OnRaycast(std::vector<RaycastResult>& dest, const Ray& ray, float maxDistance) override;
		// Add debug geometry to be rendered.
		void OnRenderDebug(DebugRenderer* debug) override;

	protected:
		// Current model resource.
		std::shared_ptr<Model> model;
		// LOD bias value.
		float lodBias;
	};

	// ==========================================================================================
	// Scene node that renders an unanimated model, which can have LOD levels.
	class StaticModel : public GeometryNode
	{
	protected:
		// Construct.
		StaticModel(Drawable* drawable);

	public:
		// Construct.
		StaticModel();
		// Destruct.
		~StaticModel();

		// Return derived drawable.
		StaticModelDrawable* GetDrawable() const { return static_cast<StaticModelDrawable*>(drawable); }

		// Set the model resource.
		void SetModel(std::shared_ptr<Model> model);
		// Set LOD bias. Values higher than 1 use higher quality LOD (acts if distance is smaller.)
		void SetLodBias(float bias);

		// Return the model resource.
		const std::shared_ptr<Model>& GetModel() const { return GetDrawable()->model; }
		// Return LOD bias.
		float LodBias() const { return GetDrawable()->lodBias; }
	};
}
