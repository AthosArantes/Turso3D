#include <Turso3D/Renderer/AnimatedModel.h>
#include <Turso3D/Core/Allocator.h>
#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Graphics/UniformBuffer.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Math/Ray.h>
#include <Turso3D/Renderer/Animation.h>
#include <Turso3D/Renderer/AnimationState.h>
#include <Turso3D/Renderer/DebugRenderer.h>
#include <Turso3D/Renderer/Model.h>
#include <Turso3D/Renderer/Octree.h>
#include <Turso3D/Resource/ResourceCache.h>
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
		animatedModelFlags(0),
		rootBone(nullptr)
	{
		SetFlag(Drawable::FLAG_SKINNED_GEOMETRY | Drawable::FLAG_OCTREE_UPDATE_CALL, true);
	}

	void AnimatedModelDrawable::OnWorldBoundingBoxUpdate() const
	{
		if (model && !bones.empty()) {
			// Recalculate bounding box from bones only if they moved individually
			if (animatedModelFlags & FLAG_BONE_BOUNDING_BOX_DIRTY) {
				const std::vector<ModelBone>& modelBones = model->Bones();

				// Use a temporary bounding box for calculations in case many threads call this simultaneously
				BoundingBox tempBox;

				for (size_t i = 0; i < bones.size(); ++i) {
					if (modelBones[i].active) {
						tempBox.Merge(modelBones[i].boundingBox.Transformed(bones[i]->WorldTransform()));
					}
				}

				worldBoundingBox = tempBox;
				boneBoundingBox = tempBox.Transformed(WorldTransform().Inverse());
				animatedModelFlags &= ~FLAG_BONE_BOUNDING_BOX_DIRTY;
			} else {
				worldBoundingBox = boneBoundingBox.Transformed(WorldTransform());
			}
		} else {
			Drawable::OnWorldBoundingBoxUpdate();
		}
	}

	void AnimatedModelDrawable::OnOctreeUpdate(unsigned short frameNumber)
	{
		if (TestFlag(Drawable::FLAG_UPDATE_INVISIBLE) || WasInView(frameNumber)) {
			if (animatedModelFlags & FLAG_ANIMATION_DIRTY) {
				UpdateAnimation();
			}
			if (animatedModelFlags & FLAG_SKINNING_DIRTY) {
				UpdateSkinning();
			}
		}
	}

	bool AnimatedModelDrawable::OnPrepareRender(unsigned short frameNumber, Camera* camera)
	{
		if (!StaticModelDrawable::OnPrepareRender(frameNumber, camera)) {
			return false;
		}

		// Update animation here too if just came into view and animation / skinning is still dirty
		if (animatedModelFlags & FLAG_ANIMATION_DIRTY) {
			UpdateAnimation();
		}
		if (animatedModelFlags & FLAG_SKINNING_DIRTY) {
			UpdateSkinning();
		}

		return true;
	}

	void AnimatedModelDrawable::OnRender(ShaderProgram*, size_t)
	{
		if (!skinMatrixBuffer || bones.empty()) {
			return;
		}

		if (animatedModelFlags & FLAG_SKINNING_BUFFER_DIRTY) {
			skinMatrixBuffer->SetData(0, bones.size() * sizeof(Matrix3x4), skinMatrices.get());
			animatedModelFlags &= ~FLAG_SKINNING_BUFFER_DIRTY;
		}

		skinMatrixBuffer->Bind(UB_OBJECTDATA);
	}

	void AnimatedModelDrawable::OnRaycast(std::vector<RaycastResult>& dest, const Ray& ray, float maxDistance_)
	{
		if (ray.HitDistance(WorldBoundingBox()) < maxDistance_ && model) {
			RaycastResult res;
			res.distance = M_INFINITY;

			// Perform raycast against each bone in its local space
			const std::vector<ModelBone>& modelBones = model->Bones();

			for (size_t i = 0; i < bones.size(); ++i) {
				if (!modelBones[i].active) {
					continue;
				}

				const Matrix3x4& transform = bones[i]->WorldTransform();
				Ray localRay = ray.Transformed(transform.Inverse());
				float localDistance = localRay.HitDistance(modelBones[i].boundingBox);

				if (localDistance < M_INFINITY) {
					// If has a hit, transform it back to world space
					Vector3 hitPosition = transform * (localRay.origin + localDistance * localRay.direction);
					float hitDistance = (hitPosition - ray.origin).Length();

					if (hitDistance < maxDistance_ && hitDistance < res.distance) {
						res.position = hitPosition;
						// TODO: Hit normal not calculated correctly
						res.normal = -ray.direction;
						res.distance = hitDistance;
						res.drawable = this;
						res.subObject = i;
					}
				}
			}

			if (res.distance < maxDistance_) {
				dest.push_back(res);
			}
		}
	}

	void AnimatedModelDrawable::OnRenderDebug(DebugRenderer* debug)
	{
		debug->AddBoundingBox(WorldBoundingBox(), Color::GREEN(), false);

		for (size_t i = 0; i < bones.size(); ++i) {
			Bone* bone = bones[i];
			// Skip the root bone, as it has no sensible connection point
			if (bone->Parent() == rootBone) {
				continue;
			}
			debug->AddLine(bone->WorldPosition(), bone->SpatialParent()->WorldPosition(), Color::WHITE(), false);
		}
	}

	void AnimatedModelDrawable::OnBoneTransformChanged()
	{
		SetFlag(Drawable::FLAG_BOUNDING_BOX_DIRTY, true);
		Octree* octree = owner->GetOctree();
		if (octree && octant && !TestFlag(Drawable::FLAG_OCTREE_REINSERT_QUEUED)) {
			octree->QueueUpdate(this);
		}
		animatedModelFlags |= FLAG_SKINNING_DIRTY | FLAG_BONE_BOUNDING_BOX_DIRTY;
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

	void AnimatedModelDrawable::CreateBones()
	{
		if (!model) {
			skinMatrixBuffer.reset();
			RemoveBones();
			return;
		}

		const std::vector<ModelBone>& modelBones = model->Bones();
		if (bones.size() != modelBones.size()) {
			RemoveBones();
		}
		bones.resize(modelBones.size());

		skinMatrices = std::make_unique<Matrix3x4[]>(bones.size());

		for (size_t i = 0; i < modelBones.size(); ++i) {
			const ModelBone& modelBone = modelBones[i];

			Node* existingBone = rootBone->FindChild(modelBone.nameHash, true);
			if (existingBone && existingBone->TestFlag(Node::FLAG_BONE)) {
				bones[i] = static_cast<Bone*>(existingBone);
			} else {
				bones[i] = new Bone();
				bones[i]->SetName(modelBone.name);
				bones[i]->SetTransform(modelBone.position, modelBone.rotation, modelBone.scale);
			}

			bones[i]->SetDrawable(this);
		}

		// Loop through bones again to set the correct parents
		for (size_t i = 0; i < modelBones.size(); ++i) {
			const ModelBone& desc = modelBones[i];
			if (desc.parentIndex == i) {
				rootBone->AddChild(std::unique_ptr<Bone>(bones[i]));
			} else {
				bones[desc.parentIndex]->AddChild(std::unique_ptr<Bone>(bones[i]));
			}
		}

		// Count child bones now for optimized transform dirtying
		for (size_t i = 0; i < modelBones.size(); ++i) {
			bones[i]->CountChildBones();
		}

		if (!skinMatrixBuffer) {
			skinMatrixBuffer = std::make_unique<UniformBuffer>();
		}
		skinMatrixBuffer->Define(USAGE_DYNAMIC, bones.size() * sizeof(Matrix3x4));

		// Set initial bone bounding box recalculation and skinning dirty.
		// Also calculate a valid bone bounding box immediately to ensure models can enter the view without updating animation first
		OnBoneTransformChanged();
		OnWorldBoundingBoxUpdate();
	}

	void AnimatedModelDrawable::RemoveBones()
	{
		if (bones.empty()) {
			return;
		}

		// Do not signal transform changes back to the model during deletion
		for (size_t i = 0; i < bones.size(); ++i) {
			bones[i]->SetDrawable(nullptr);
		}
		bones.clear();

		skinMatrices.reset();
		skinMatrixBuffer.reset();
	}

	void AnimatedModelDrawable::UpdateAnimation()
	{
		if (animatedModelFlags & FLAG_ANIMATION_ORDER_DIRTY) {
			std::sort(animationStates.begin(), animationStates.end(), CompareAnimationStates);
		}
		animatedModelFlags |= FLAG_IN_ANIMATION_UPDATE | FLAG_BONE_BOUNDING_BOX_DIRTY;

		// Reset bones to initial pose, then apply animations
		const std::vector<ModelBone>& modelBones = model->Bones();
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
		SetBoneTransformsDirty();

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

		animatedModelFlags |= FLAG_SKINNING_DIRTY;
	}

	void AnimatedModelDrawable::UpdateSkinning()
	{
		const std::vector<ModelBone>& modelBones = model->Bones();
		for (size_t i = 0; i < bones.size(); ++i) {
			skinMatrices[i] = bones[i]->WorldTransform() * modelBones[i].offsetMatrix;
		}
		animatedModelFlags &= ~FLAG_SKINNING_DIRTY;
		animatedModelFlags |= FLAG_SKINNING_BUFFER_DIRTY;
	}

	void AnimatedModelDrawable::SetBoneTransformsDirty()
	{
		rootBone->SetFlag(Node::FLAG_WORLDTRANSFORMDIRTY, true);
		for (size_t i = 0; i < bones.size(); ++i) {
			// If bone has only other bones as children, just set its world transform dirty without going through the hierarchy.
			// The whole hierarchy will be eventually updated
			if (bones[i]->NumChildren() == bones[i]->NumChildBones()) {
				bones[i]->SetFlag(Node::FLAG_WORLDTRANSFORMDIRTY, true);
			} else {
				bones[i]->OnTransformChanged();
			}
		}
	}

	void AnimatedModelDrawable::SetRootBone(Bone* bone)
	{
		rootBone = bone;
	}

	// ==========================================================================================
	Bone::Bone() :
		drawable(nullptr),
		animationEnabled(true),
		numChildBones(0)
	{
		SetFlag(FLAG_BONE, true);
	}

	Bone::~Bone()
	{
	}

	void Bone::SetDrawable(AnimatedModelDrawable* drawable_)
	{
		drawable = drawable_;
	}

	void Bone::SetAnimationEnabled(bool enable)
	{
		animationEnabled = enable;
	}

	void Bone::CountChildBones()
	{
		numChildBones = 0;
		for (size_t i = 0; i < children.size(); ++i) {
			Node* child = children[i].get();
			if (child->TestFlag(FLAG_BONE)) {
				++numChildBones;
			}
		}
	}

	void Bone::SetTransformSilent(const Vector3& position, const Quaternion& rotation, const Vector3& scale)
	{
		this->position = position;
		this->rotation = rotation;
		this->scale = scale;
	}

	void Bone::OnTransformChanged()
	{
		SpatialNode::OnTransformChanged();

		// Avoid duplicate dirtying calls if model's skinning is already dirty.
		// Do not signal changes either during animation update,
		// as the model will set the hierarchy dirty when finished.
		// This is also used to optimize when only the model node moves.
		if (drawable && !(drawable->AnimatedModelFlags() & (AnimatedModelDrawable::FLAG_IN_ANIMATION_UPDATE | AnimatedModelDrawable::FLAG_SKINNING_DIRTY))) {
			drawable->OnBoneTransformChanged();
		}
	}

	// ==========================================================================================
	AnimatedModel::AnimatedModel() :
		StaticModel(drawableAllocator.Allocate())
	{
		// Create a nameless bone used to contain all models' bones.
		Bone* rootBone = CreateChild<Bone>();
		GetDrawable()->SetRootBone(rootBone);
	}

	AnimatedModel::~AnimatedModel()
	{
		AnimatedModelDrawable* drawable = GetDrawable();
		drawable->RemoveBones();
		RemoveFromOctree();
		drawableAllocator.Free(drawable);
		this->drawable = nullptr;
	}

	void AnimatedModel::SetModel(std::shared_ptr<Model> model)
	{
		StaticModel::SetModel(model);
		GetDrawable()->CreateBones();
	}

	AnimationState* AnimatedModel::AddAnimationState(const std::shared_ptr<Animation>& animation)
	{
		AnimatedModelDrawable* drawable = GetDrawable();

		if (!animation || drawable->bones.empty()) {
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

	void AnimatedModel::OnSceneSet(Scene* newScene, Scene* oldScene)
	{
		OctreeNode::OnSceneSet(newScene, oldScene);

		// Set octree also directly to the drawable so it can queue itself
		//static_cast<AnimatedModelDrawable*>(drawable)->octree = octree;
	}

	void AnimatedModel::OnTransformChanged()
	{
		AnimatedModelDrawable* drawable = GetDrawable();

		// To improve performance set skinning dirty now, so the bone nodes will not redundantly signal transform changes back
		drawable->animatedModelFlags |= AnimatedModelDrawable::FLAG_SKINNING_DIRTY;

		// If have other children than the root bone, dirty the hierarchy normally. Otherwise optimize
		if (children.size() > 1) {
			SpatialNode::OnTransformChanged();
		} else {
			drawable->SetBoneTransformsDirty();
			drawable->SetFlag(Drawable::FLAG_WORLD_TRANSFORM_DIRTY, true);
			SetFlag(FLAG_WORLDTRANSFORMDIRTY, true);
		}

		drawable->SetFlag(Drawable::FLAG_BOUNDING_BOX_DIRTY, true);
		if (octree && drawable->octant && !drawable->TestFlag(Drawable::FLAG_OCTREE_REINSERT_QUEUED)) {
			octree->QueueUpdate(drawable);
		}
	}
}
