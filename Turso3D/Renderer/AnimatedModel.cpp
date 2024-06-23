#include <Turso3D/Core/Allocator.h>
#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Graphics/UniformBuffer.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Math/Ray.h>
#include <Turso3D/Renderer/AnimatedModel.h>
#include <Turso3D/Renderer/Animation.h>
#include <Turso3D/Renderer/AnimationState.h>
#include <Turso3D/Renderer/DebugRenderer.h>
#include <Turso3D/Renderer/Model.h>
#include <Turso3D/Renderer/Octree.h>
#include <algorithm>

namespace
{
	using namespace Turso3D;

	static Allocator<AnimatedModelDrawable> drawableAllocator;

	static inline bool CompareAnimationStates(const std::shared_ptr<AnimationState>& lhs, const std::shared_ptr<AnimationState>& rhs)
	{
		return lhs->BlendLayer() < rhs->BlendLayer();
	}
}

namespace Turso3D
{
	AnimatedModelDrawable::AnimatedModelDrawable() :
		animatedModelFlags(0)
	{
	}

	AnimatedModelDrawable::~AnimatedModelDrawable()
	{
	}

	void AnimatedModelDrawable::OnOctreeUpdate(unsigned short frameNumber)
	{
		if (TestFlag(Drawable::FLAG_UPDATE_INVISIBLE) || WasInView(frameNumber)) {
			if (animatedModelFlags & FLAG_ANIMATION_DIRTY) {
				UpdateAnimation();
			}
		}
		SkinnedModelDrawable::OnOctreeUpdate(frameNumber);
	}

	void AnimatedModelDrawable::OnAnimationOrderChanged()
	{
		Octree* octree = owner->GetOctree();
		if (octree && octant && !TestFlag(Drawable::FLAG_OCTREE_REINSERT_QUEUED)) {
			octree->QueueUpdate(this);
		}
		animatedModelFlags |= FLAG_ANIMATION_DIRTY | FLAG_ANIMATION_ORDER_DIRTY;
	}

	void AnimatedModelDrawable::OnAnimationChanged()
	{
		Octree* octree = owner->GetOctree();
		if (octree && octant && !TestFlag(Drawable::FLAG_OCTREE_REINSERT_QUEUED)) {
			octree->QueueUpdate(this);
		}
		animatedModelFlags |= FLAG_ANIMATION_DIRTY;
	}

	void AnimatedModelDrawable::UpdateAnimation()
	{
		if (animatedModelFlags & FLAG_ANIMATION_ORDER_DIRTY) {
			std::sort(animationStates.begin(), animationStates.end(), CompareAnimationStates);
		}
		animatedModelFlags |= FLAG_IN_ANIMATION_UPDATE;

		// Reset bones to initial pose, then apply animations
		const std::vector<ModelBone>& modelBones = model->Bones();

		const std::vector<Bone*>& bones = Bones();
		for (size_t i = 0; i < bones.size(); ++i) {
			Bone* bone = bones[i];
			const ModelBone& modelBone = modelBones[i];
			if (bone->AnimationEnabled()) {
				bone->SetTransformSilent(modelBone.position, modelBone.rotation, modelBone.scale);
			}
		}

		for (size_t i = 0; i < animationStates.size(); ++i) {
			AnimationState* state = animationStates[i].get();
			if (state->Enabled()) {
				state->Apply();
			}
		}

		// Dirty the bone hierarchy now.
		// This will also dirty and queue reinsertion for attached models
		static_cast<AnimatedModel*>(owner)->SetBonesDirty();

		animatedModelFlags &= ~(FLAG_ANIMATION_ORDER_DIRTY | FLAG_ANIMATION_DIRTY | FLAG_IN_ANIMATION_UPDATE);

		// Update bounding box already here to take advantage of threaded update, and also to update bone world transforms for skinning
		OnWorldBoundingBoxUpdate();

		// If updating only when visible, queue octree reinsertion for next frame. This also ensures shadowmap rendering happens correctly
		// Else just dirty the skinning
		if (!TestFlag(Drawable::FLAG_UPDATE_INVISIBLE)) {
			Octree* octree = owner->GetOctree();
			if (octree && octant && !TestFlag(Drawable::FLAG_OCTREE_REINSERT_QUEUED)) {
				octree->QueueUpdate(this);
			}
		}

		skinFlags |= FLAG_SKINNING_DIRTY | FLAG_BONE_BOUNDING_BOX_DIRTY;
	}

	void AnimatedModelDrawable::PrepareForRender()
	{
		// Update animation here too if just came into view and animation / skinning is still dirty
		if (animatedModelFlags & FLAG_ANIMATION_DIRTY) {
			UpdateAnimation();
		}
		SkinnedModelDrawable::PrepareForRender();
	}

	// ==========================================================================================
	AnimatedModel::AnimatedModel() :
		SkinnedModel(drawableAllocator.Allocate())
	{
	}

	AnimatedModel::~AnimatedModel()
	{
		if (drawable) {
			RemoveFromOctree();
			drawableAllocator.Free(static_cast<AnimatedModelDrawable*>(drawable));
			drawable = nullptr;
		}
	}

	void AnimatedModel::SetModel(std::shared_ptr<Model> model)
	{
		StaticModel::SetModel(model);
		SetupBones();
	}

	AnimationState* AnimatedModel::AddAnimationState(const std::shared_ptr<Animation>& animation)
	{
		AnimatedModelDrawable* drawable = GetDrawable();

		if (!animation || bones.empty()) {
			return nullptr;
		}

		// Check for not adding twice
		AnimationState* existing = FindAnimationState(animation.get());
		if (existing) {
			return existing;
		}

		std::shared_ptr<AnimationState> newState = std::make_shared<AnimationState>(drawable, animation);
		drawable->animationStates.push_back(newState);
		drawable->OnAnimationOrderChanged();

		return newState.get();
	}

	void AnimatedModel::RemoveAnimationState(Animation* animation)
	{
		if (animation) {
			RemoveAnimationState(animation->NameHash());
		}
	}

	void AnimatedModel::RemoveAnimationState(StringHash animationNameHash)
	{
		AnimatedModelDrawable* drawable = GetDrawable();
		std::vector<std::shared_ptr<AnimationState>>& anim_states = drawable->animationStates;

		for (size_t i = 0; i < anim_states.size(); ++i) {
			AnimationState* state = anim_states[i].get();
			Animation* animation = state->GetAnimation().get();

			// Check both the animation and the resource name
			if (animation->NameHash() == animationNameHash || animation->AnimationNameHash() == animationNameHash) {
				anim_states[i].swap(anim_states.back());
				anim_states.pop_back();
				drawable->OnAnimationChanged();
				return;
			}
		}
	}

	void AnimatedModel::RemoveAnimationState(AnimationState* state)
	{
		AnimatedModelDrawable* drawable = GetDrawable();
		std::vector<std::shared_ptr<AnimationState>>& anim_states = drawable->animationStates;

		for (size_t i = 0; i < anim_states.size(); ++i) {
			if (anim_states[i].get() == state) {
				anim_states[i].swap(anim_states.back());
				anim_states.pop_back();
				drawable->OnAnimationChanged();
				return;
			}
		}
	}

	void AnimatedModel::RemoveAnimationState(size_t index)
	{
		AnimatedModelDrawable* drawable = GetDrawable();
		std::vector<std::shared_ptr<AnimationState>>& anim_states = drawable->animationStates;

		if (index < anim_states.size()) {
			anim_states[index].swap(anim_states.back());
			anim_states.pop_back();
			drawable->OnAnimationChanged();
		}
	}

	void AnimatedModel::RemoveAllAnimationStates()
	{
		AnimatedModelDrawable* drawable = GetDrawable();
		if (drawable->animationStates.size()) {
			drawable->animationStates.clear();
			drawable->OnAnimationChanged();
		}
	}

	AnimationState* AnimatedModel::FindAnimationState(Animation* animation) const
	{
		AnimatedModelDrawable* drawable = GetDrawable();
		std::vector<std::shared_ptr<AnimationState>>& anim_states = drawable->animationStates;
		for (size_t i = 0; i < anim_states.size(); ++i) {
			if (anim_states[i]->GetAnimation().get() == animation) {
				return anim_states[i].get();
			}
		}
		return nullptr;
	}

	AnimationState* AnimatedModel::FindAnimationState(StringHash animationNameHash) const
	{
		AnimatedModelDrawable* drawable = GetDrawable();
		std::vector<std::shared_ptr<AnimationState>>& anim_states = drawable->animationStates;
		for (size_t i = 0; i < anim_states.size(); ++i) {
			Animation* animation = anim_states[i]->GetAnimation().get();
			// Check both the animation and the resource name
			if (animation->NameHash() == animationNameHash || animation->AnimationNameHash() == animationNameHash) {
				return anim_states[i].get();
			}
		}
		return nullptr;
	}

	AnimationState* AnimatedModel::GetAnimationState(size_t index) const
	{
		AnimatedModelDrawable* drawable = GetDrawable();
		return index < drawable->animationStates.size() ? drawable->animationStates[index].get() : nullptr;
	}

	void AnimatedModel::AddAttachment(SkinnedModel* model)
	{
		for (size_t i = 0; i < attachments.size(); ++i) {
			if (attachments[i] == model) {
				return;
			}
		}
		attachments.push_back(model);
		model->SetSkinningDirty();
	}

	void AnimatedModel::RemoveAttachment(SkinnedModel* attachment)
	{
		for (size_t i = 0; i < attachments.size(); ++i) {
			if (attachments[i] == attachment) {
				std::swap(attachments.back(), attachments[i]);
				attachments.pop_back();
				return;
			}
		}
	}

	void AnimatedModel::OnBoneTransformChanged(Bone* bone)
	{
		SkinnedModel::OnBoneTransformChanged(bone);

		for (size_t i = 0; i < attachments.size(); ++i) {
			SkinnedModel* attachment = attachments[i];

			if (bone && bone != rootBone) {
				const std::vector<Bone*>& bones = attachment->Bones();
				for (size_t j = 0; j < bones.size(); ++j) {
					if (bones[i]->NameHash() == bone->NameHash()) {
						attachment->SetSkinningDirty();
						break;
					}
				}
			} else {
				attachment->SetSkinningDirty();
			}
		}
	}
}
