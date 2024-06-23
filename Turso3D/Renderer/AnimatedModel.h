#pragma once

#include <Turso3D/Renderer/SkinnedModel.h>
#include <vector>
#include <memory>

namespace Turso3D
{
	class AnimatedModel;
	class Animation;
	class AnimationState;

	// ==========================================================================================
	// Animated model drawable.
	class AnimatedModelDrawable : public SkinnedModelDrawable
	{
		friend class AnimatedModel;

	public:
		enum Flag
		{
			FLAG_ANIMATION_ORDER_DIRTY = 0x1,
			FLAG_ANIMATION_DIRTY = 0x2,
			FLAG_IN_ANIMATION_UPDATE = 0x4
		};

	public:
		// Construct.
		AnimatedModelDrawable();
		~AnimatedModelDrawable();

		// Do animation processing before octree reinsertion, if should update without regard to visibility.
		// Called by Octree in worker threads. Must be opted-in by setting FLAG_OCTREE_UPDATE_CALL flag.
		void OnOctreeUpdate(unsigned short frameNumber) override;

		// Set animation order dirty when animation state changes layer order and queue octree reinsertion.
		// Note: bounding box will only be dirtied once animation actually updates.
		void OnAnimationOrderChanged();
		// Set animation dirty when animation state changes time position or weight and queue octree reinsertion.
		// Note: bounding box will only be dirtied once animation actually updates.
		void OnAnimationChanged();

		// Apply animation states and recalculate bounding box.
		void UpdateAnimation();

	protected:
		void PrepareForRender() override;

	protected:
		// Internal dirty status flags.
		mutable unsigned animatedModelFlags;

		// Animation states.
		std::vector<std::shared_ptr<AnimationState>> animationStates;
	};

	// ==========================================================================================
	// Scene node that renders a skeletally animated (skinned) model.
	class AnimatedModel : public SkinnedModel
	{
	public:
		// Construct.
		AnimatedModel();
		// Destruct.
		~AnimatedModel();

		// Return derived drawable.
		AnimatedModelDrawable* GetDrawable() const { return static_cast<AnimatedModelDrawable*>(drawable); }

		// Set the model resource and create / acquire bone scene nodes.
		void SetModel(std::shared_ptr<Model> model);

		// Add an animation and return the created animation state.
		AnimationState* AddAnimationState(const std::shared_ptr<Animation>& animation);
		// Remove an animation by animation pointer.
		void RemoveAnimationState(Animation* animation);
		// Remove an animation by animation name hash.
		void RemoveAnimationState(StringHash animationNameHash);
		// Remove an animation by AnimationState pointer.
		void RemoveAnimationState(AnimationState* state);
		// Remove an animation by state index.
		void RemoveAnimationState(size_t index);
		// Remove all animations.
		void RemoveAllAnimationStates();

		// Return animation state by index.
		AnimationState* GetAnimationState(size_t index) const;
		// Return animation state by animation pointer.
		AnimationState* FindAnimationState(Animation* animation) const;
		// Return animation state by animation name hash.
		AnimationState* FindAnimationState(StringHash animationNameHash) const;

		// Return all animation states.
		const std::vector<std::shared_ptr<AnimationState>>& AnimationStates() const { return GetDrawable()->animationStates; }

		// Add a skinned model as attachment to this model.
		void AddAttachment(SkinnedModel* model);
		// Remove an attached skinned model.
		void RemoveAttachment(SkinnedModel* model);
		// Return all attachments.
		const std::vector<SkinnedModel*>& GetAttachments() const { return attachments; }

	protected:
		// Called when a bone had its transform changed.
		void OnBoneTransformChanged(Bone* bone) override;

	protected:
		std::vector<SkinnedModel*> attachments;
	};
}
