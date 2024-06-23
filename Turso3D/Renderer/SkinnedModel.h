#pragma once

#include <Turso3D/Renderer/StaticModel.h>
#include <memory>
#include <vector>

namespace Turso3D
{
	class Bone;
	class SkinnedModel;
	class UniformBuffer;

	// Base class for drawables that is affected by a bone hierarchy.
	class SkinnedModelDrawable : public StaticModelDrawable
	{
		friend class SkinnedModel;

	public:
		enum Flag
		{
			FLAG_SKINNING_DIRTY = 0x1,
			FLAG_SKINNING_BUFFER_DIRTY = 0x2,
			FLAG_BONE_BOUNDING_BOX_DIRTY = 0x4,
			FLAG_APPLY_PARENT_TRANSFORM = 0x8
		};

	public:
		SkinnedModelDrawable();
		~SkinnedModelDrawable();

		// Recalculate the world space bounding box.
		void OnWorldBoundingBoxUpdate() const override;
		// Do animation processing before octree reinsertion, if should update without regard to visibility.
		// Called by Octree in worker threads. Must be opted-in by setting FLAG_OCTREE_UPDATE_CALL flag.
		void OnOctreeUpdate(unsigned short frameNumber) override;
		// Prepare object for rendering.
		// Reset framenumber and calculate distance from camera, check for LOD level changes, and update skinning if necessary.
		// Called by Renderer in worker threads.
		// Return false if should not render.
		bool OnPrepareRender(unsigned short frameNumber, Camera* camera) override;
		// Update GPU resources and set uniforms for rendering.
		// Called by Renderer when geometry type is not static.
		void OnRender(ShaderProgram* program, size_t geomIndex) override;
		// Perform ray test on self and add possible hit to the result vector.
		void OnRaycast(std::vector<RaycastResult>& dest, const Ray& ray, float maxDistance) override;
		// Add debug geometry to be rendered.
		void OnRenderDebug(DebugRenderer* debug) override;

		// Update skin matrices for rendering.
		void UpdateSkinning();

		// Return the bone node references from the owner node.
		const std::vector<Bone*>& Bones() const;
		// Return the root bone from the owner node.
		Bone* RootBone() const;

	protected:
		// Called in OnPrepareRender() when the drawable must be rendered.
		virtual void PrepareForRender();

	protected:
		// Combined bounding box of the bones in model space, used for quick updates when only the node moves without animation
		mutable BoundingBox boneBoundingBox;
		// Internal state flags.
		mutable unsigned skinFlags;

		// Skinning uniform buffer.
		std::unique_ptr<UniformBuffer> skinMatrixBuffer;
		// Skinning uniform buffer data.
		std::unique_ptr<Matrix3x4[]> skinMatrices;
	};

	// ==========================================================================================
	struct BoneListener
	{
		virtual void OnBoneTransformChanged(Bone* bone) = 0;

		bool boneListening;
	};

	// Bone scene node for skeletal animation.
	class Bone : public SpatialNode
	{
	public:
		// Construct.
		Bone();
		// Destruct.
		~Bone();

		// Set the listener for bone transformation changes.
		void SetListener(BoneListener* newListener);
		// Return the bone listener.
		BoneListener* Listener() const { return listener; }

		// Set animation enabled.
		// Default is enabled, when disabled the bone can be programmatically controlled.
		void SetAnimationEnabled(bool enable);
		// Return whether animation is enabled.
		bool AnimationEnabled() const { return animationEnabled; }

		// Count number of child bones.
		// Called by SkinnedModel once the skeleton has been fully created.
		void CountChildBones();
		// Return amount of child bones.
		// This is used to check whether bone has attached objects and its dirtying cannot be handled in an optimized way.
		size_t NumChildBones() const { return numChildBones; }

		// Set bone parent space transform without dirtying the hierarchy.
		void SetTransformSilent(const Vector3& position, const Quaternion& rotation, const Vector3& scale);
		// Optimally set the world transform dirty.
		void SetTransformDirty();

	protected:
		// Handle the transform matrix changing.
		void OnTransformChanged() override;

	private:
		// Associated listener for receiving bone transform changes.
		BoneListener* listener;
		// Amount of child bones.
		size_t numChildBones;
		// Animation enabled flag.
		bool animationEnabled;
	};

	// ==========================================================================================
	// Base scene node that renders geometry affected by a bone hierarchy.
	class SkinnedModel : public StaticModel, protected BoneListener
	{
	protected:
		// Construct.
		SkinnedModel(Drawable* drawable);

	public:
		// Construct.
		SkinnedModel();
		// Destruct.
		~SkinnedModel();

		// Return derived drawable.
		SkinnedModelDrawable* GetDrawable() const { return static_cast<SkinnedModelDrawable*>(drawable); }

		// Sets whether to apply parent transform when calculating skin matrices.
		void SetUseParentTransform(bool enable);
		// Gets whether parent transform is being used when calculating skin matrices.
		bool IsUsingParentTransform() const { return GetDrawable()->skinFlags & SkinnedModelDrawable::FLAG_APPLY_PARENT_TRANSFORM; }

		// Create bone scene nodes from the model.
		// If compatible bones already exist in the root hierarchy, they are taken into use instead of creating new.
		// If root is nullptr, a new one will be created as child of this model node.
		void SetupBones(Bone* root = nullptr);

		// Return the root bone.
		Bone* RootBone() const { return rootBone; }
		// Return the bone references in the node hierarchy.
		const std::vector<Bone*>& Bones() const { return bones; }

		// Set bounding box and skinning dirty and queue octree reinsertion.
		void SetSkinningDirty();
		// Optimally set all bones transformation dirty.
		// no-op if this skinned model doesn't own the root bone.
		void SetBonesDirty();

	protected:
		// Handle the transform matrix changing.
		void OnTransformChanged() override;
		// Set skinning dirty when any of the bones move.
		void OnBoneTransformChanged(Bone* bone) override;

	protected:
		// Root bone containing all model bone hierarchy.
		Bone* rootBone;
		// Model bone references in the node hierarchy.
		std::vector<Bone*> bones;
	};
}
