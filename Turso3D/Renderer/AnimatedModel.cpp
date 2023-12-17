#include "AnimatedModel.h"
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

namespace Turso3D
{
	static Allocator<AnimatedModelDrawable> drawableAllocator;

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
		for (const std::unique_ptr<Node>& child : children) {
			if (child->TestFlag(FLAG_BONE)) {
				++numChildBones;
			}
		}
	}

	void Bone::OnTransformChanged()
	{
		SpatialNode::OnTransformChanged();

		// Avoid duplicate dirtying calls if model's skinning is already dirty.
		// Do not signal changes either during animation update,
		// as the model will set the hierarchy dirty when finished.
		// This is also used to optimize when only the model node moves.
		if (drawable && !(drawable->AnimatedModelFlags() & (AMF_IN_ANIMATION_UPDATE | AMF_SKINNING_DIRTY))) {
			drawable->OnBoneTransformChanged();
		}
	}

	// ==========================================================================================
	static inline bool CompareAnimationStates(const std::shared_ptr<AnimationState>& lhs, const std::shared_ptr<AnimationState>& rhs)
	{
		return lhs->BlendLayer() < rhs->BlendLayer();
	}

	AnimatedModelDrawable::AnimatedModelDrawable() :
		animatedModelFlags(0),
		numBones(0),
		octree(nullptr),
		rootBone(nullptr)
	{
		SetFlag(Drawable::FLAG_SKINNED_GEOMETRY | Drawable::FLAG_OCTREE_UPDATE_CALL, true);
	}

	void AnimatedModelDrawable::OnWorldBoundingBoxUpdate() const
	{
		if (model && numBones) {
			// Recalculate bounding box from bones only if they moved individually
			if (animatedModelFlags & AMF_BONE_BOUNDING_BOX_DIRTY) {
				const std::vector<ModelBone>& modelBones = model->Bones();

				// Use a temporary bounding box for calculations in case many threads call this simultaneously
				BoundingBox tempBox;

				for (size_t i = 0; i < numBones; ++i) {
					if (modelBones[i].active) {
						tempBox.Merge(modelBones[i].boundingBox.Transformed(bones[i]->WorldTransform()));
					}
				}

				worldBoundingBox = tempBox;
				boneBoundingBox = tempBox.Transformed(WorldTransform().Inverse());
				animatedModelFlags &= ~AMF_BONE_BOUNDING_BOX_DIRTY;
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
			if (animatedModelFlags & AMF_ANIMATION_DIRTY) {
				UpdateAnimation();
			}
			if (animatedModelFlags & AMF_SKINNING_DIRTY) {
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
		if (animatedModelFlags & AMF_ANIMATION_DIRTY) {
			UpdateAnimation();
		}
		if (animatedModelFlags & AMF_SKINNING_DIRTY) {
			UpdateSkinning();
		}

		return true;
	}

	void AnimatedModelDrawable::OnRender(ShaderProgram*, size_t)
	{
		if (!skinMatrixBuffer || !numBones) {
			return;
		}

		if (animatedModelFlags & AMF_SKINNING_BUFFER_DIRTY) {
			skinMatrixBuffer->SetData(0, numBones * sizeof(Matrix3x4), skinMatrices.get());
			animatedModelFlags &= ~AMF_SKINNING_BUFFER_DIRTY;
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

			for (size_t i = 0; i < numBones; ++i) {
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
		debug->AddBoundingBox(WorldBoundingBox(), Color::GREEN, false);

		for (size_t i = 0; i < numBones; ++i) {
			// Skip the root bone, as it has no sensible connection point
			Bone* bone = bones[i];
			if (bone != rootBone) {
				debug->AddLine(bone->WorldPosition(), bone->SpatialParent()->WorldPosition(), Color::WHITE, false);
			}
		}
	}

	void AnimatedModelDrawable::CreateBones()
	{
		if (!model) {
			skinMatrixBuffer.reset();
			RemoveBones();
			return;
		}

		const std::vector<ModelBone>& modelBones = model->Bones();
		if (numBones != modelBones.size()) {
			RemoveBones();
		}

		numBones = (unsigned short)modelBones.size();

		bones = std::make_unique<Bone*[]>(numBones);
		skinMatrices = std::make_unique<Matrix3x4[]>(numBones);

		for (size_t i = 0; i < modelBones.size(); ++i) {
			const ModelBone& modelBone = modelBones[i];

			Node* existingBone = owner->FindChild(modelBone.nameHash, true);
			if (existingBone && existingBone->TestFlag(Node::FLAG_BONE)) {
				bones[i] = static_cast<Bone*>(existingBone);
			} else {
				bones[i] = new Bone();
				bones[i]->SetName(modelBone.name);
				bones[i]->SetTransform(modelBone.initialPosition, modelBone.initialRotation, modelBone.initialScale);
			}

			bones[i]->SetDrawable(this);
		}

		// Loop through bones again to set the correct parents
		for (size_t i = 0; i < modelBones.size(); ++i) {
			const ModelBone& desc = modelBones[i];
			if (desc.parentIndex == i) {
				owner->AddChild(std::unique_ptr<Bone>(bones[i]));
				rootBone = bones[i];
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
		skinMatrixBuffer->Define(USAGE_DYNAMIC, numBones * sizeof(Matrix3x4));

		// Set initial bone bounding box recalculation and skinning dirty. Also calculate a valid bone bounding box immediately to ensure models can enter the view without updating animation first
		OnBoneTransformChanged();
		OnWorldBoundingBoxUpdate();
	}

	void AnimatedModelDrawable::UpdateAnimation()
	{
		if (animatedModelFlags & AMF_ANIMATION_ORDER_DIRTY) {
			std::sort(animationStates.begin(), animationStates.end(), CompareAnimationStates);
		}
		animatedModelFlags |= AMF_IN_ANIMATION_UPDATE | AMF_BONE_BOUNDING_BOX_DIRTY;

		// Reset bones to initial pose, then apply animations
		const std::vector<ModelBone>& modelBones = model->Bones();

		for (size_t i = 0; i < numBones; ++i) {
			Bone* bone = bones[i];
			const ModelBone& modelBone = modelBones[i];
			if (bone->AnimationEnabled()) {
				bone->SetTransformSilent(modelBone.initialPosition, modelBone.initialRotation, modelBone.initialScale);
			}
		}

		for (auto it = animationStates.begin(); it != animationStates.end(); ++it) {
			AnimationState* state = it->get();
			if (state->Enabled()) {
				state->Apply();
			}
		}

		// Dirty the bone hierarchy now. This will also dirty and queue reinsertion for attached models
		SetBoneTransformsDirty();

		animatedModelFlags &= ~(AMF_ANIMATION_ORDER_DIRTY | AMF_ANIMATION_DIRTY | AMF_IN_ANIMATION_UPDATE);

		// Update bounding box already here to take advantage of threaded update, and also to update bone world transforms for skinning
		OnWorldBoundingBoxUpdate();

		// If updating only when visible, queue octree reinsertion for next frame. This also ensures shadowmap rendering happens correctly
		// Else just dirty the skinning
		if (!TestFlag(Drawable::FLAG_UPDATE_INVISIBLE)) {
			if (octree && octant && !TestFlag(Drawable::FLAG_OCTREE_REINSERT_QUEUED)) {
				octree->QueueUpdate(this);
			}
		}

		animatedModelFlags |= AMF_SKINNING_DIRTY;
	}

	void AnimatedModelDrawable::UpdateSkinning()
	{
		const std::vector<ModelBone>& modelBones = model->Bones();

		for (size_t i = 0; i < numBones; ++i) {
			skinMatrices[i] = bones[i]->WorldTransform() * modelBones[i].offsetMatrix;
		}
		animatedModelFlags &= ~AMF_SKINNING_DIRTY;
		animatedModelFlags |= AMF_SKINNING_BUFFER_DIRTY;
	}

	void AnimatedModelDrawable::SetBoneTransformsDirty()
	{
		for (size_t i = 0; i < numBones; ++i) {
			// If bone has only other bones as children, just set its world transform dirty without going through the hierarchy.
			// The whole hierarchy will be eventually updated
			if (bones[i]->NumChildren() == bones[i]->NumChildBones()) {
				bones[i]->SetFlag(Node::FLAG_WORLDTRANSFORMDIRTY, true);
			} else {
				bones[i]->OnTransformChanged();
			}
		}
	}

	void AnimatedModelDrawable::RemoveBones()
	{
		if (!numBones) {
			return;
		}

		// Do not signal transform changes back to the model during deletion
		for (size_t i = 0; i < numBones; ++i) {
			bones[i]->SetDrawable(nullptr);
		}

		if (rootBone) {
			rootBone->RemoveSelf();
			rootBone = nullptr;
		}

		bones.reset();
		skinMatrices.reset();
		skinMatrixBuffer.reset();
		numBones = 0;
	}

	// ==========================================================================================
	AnimatedModel::AnimatedModel()
	{
		drawable = drawableAllocator.Allocate();
		drawable->SetOwner(this);
	}

	AnimatedModel::~AnimatedModel()
	{
		static_cast<AnimatedModelDrawable*>(drawable)->RemoveBones();
		RemoveFromOctree();
		drawableAllocator.Free(static_cast<AnimatedModelDrawable*>(drawable));
		drawable = nullptr;
	}

	void AnimatedModel::SetModel(const std::shared_ptr<Model>& model_)
	{
		StaticModel::SetModel(model_);
		static_cast<AnimatedModelDrawable*>(drawable)->CreateBones();
	}

	AnimationState* AnimatedModel::AddAnimationState(const std::shared_ptr<Animation>& animation)
	{
		AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);

		if (!animation || !modelDrawable->numBones) {
			return nullptr;
		}

		// Check for not adding twice
		AnimationState* existing = FindAnimationState(animation.get());
		if (existing) {
			return existing;
		}

		std::shared_ptr<AnimationState> newState = std::make_shared<AnimationState>(modelDrawable, animation);
		modelDrawable->animationStates.push_back(newState);
		modelDrawable->OnAnimationOrderChanged();

		return newState.get();
	}

	void AnimatedModel::RemoveAnimationState(Animation* animation)
	{
		if (animation) {
			RemoveAnimationState(animation->NameHash());
		}
	}

	void AnimatedModel::RemoveAnimationState(const std::string& animationName)
	{
		RemoveAnimationState(StringHash(animationName));
	}

	void AnimatedModel::RemoveAnimationState(const char* animationName)
	{
		RemoveAnimationState(StringHash(animationName));
	}

	void AnimatedModel::RemoveAnimationState(StringHash animationNameHash)
	{
		AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);

		for (auto it = modelDrawable->animationStates.begin(); it != modelDrawable->animationStates.end(); ++it) {
			AnimationState* state = it->get();
			Animation* animation = state->GetAnimation().get();
			// Check both the animation and the resource name
			if (animation->NameHash() == animationNameHash || animation->AnimationNameHash() == animationNameHash) {
				modelDrawable->animationStates.erase(it);
				modelDrawable->OnAnimationChanged();
				return;
			}
		}
	}

	void AnimatedModel::RemoveAnimationState(AnimationState* state)
	{
		AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);

		for (auto it = modelDrawable->animationStates.begin(); it != modelDrawable->animationStates.end(); ++it) {
			if (it->get() == state) {
				modelDrawable->animationStates.erase(it);
				modelDrawable->OnAnimationChanged();
				return;
			}
		}
	}

	void AnimatedModel::RemoveAnimationState(size_t index)
	{
		AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);

		if (index < modelDrawable->animationStates.size()) {
			modelDrawable->animationStates.erase(modelDrawable->animationStates.begin() + index);
			modelDrawable->OnAnimationChanged();
		}
	}

	void AnimatedModel::RemoveAllAnimationStates()
	{
		AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);

		if (modelDrawable->animationStates.size()) {
			modelDrawable->animationStates.clear();
			modelDrawable->OnAnimationChanged();
		}
	}

	AnimationState* AnimatedModel::FindAnimationState(Animation* animation) const
	{
		AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);
		for (auto it = modelDrawable->animationStates.begin(); it != modelDrawable->animationStates.end(); ++it) {
			if ((*it)->GetAnimation().get() == animation) {
				return it->get();
			}
		}
		return nullptr;
	}

	AnimationState* AnimatedModel::FindAnimationState(const std::string& animationName) const
	{
		return GetAnimationState(StringHash(animationName));
	}

	AnimationState* AnimatedModel::FindAnimationState(const char* animationName) const
	{
		return GetAnimationState(StringHash(animationName));
	}

	AnimationState* AnimatedModel::FindAnimationState(StringHash animationNameHash) const
	{
		AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);
		for (auto it = modelDrawable->animationStates.begin(); it != modelDrawable->animationStates.end(); ++it) {
			Animation* animation = (*it)->GetAnimation().get();
			// Check both the animation and the resource name
			if (animation->NameHash() == animationNameHash || animation->AnimationNameHash() == animationNameHash) {
				return it->get();
			}
		}
		return nullptr;
	}

	AnimationState* AnimatedModel::GetAnimationState(size_t index) const
	{
		AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);
		return index < modelDrawable->animationStates.size() ? modelDrawable->animationStates[index].get() : nullptr;
	}

	void AnimatedModel::OnSceneSet(Scene* newScene, Scene* oldScene)
	{
		OctreeNode::OnSceneSet(newScene, oldScene);

		// Set octree also directly to the drawable so it can queue itself
		static_cast<AnimatedModelDrawable*>(drawable)->octree = octree;
	}

	void AnimatedModel::OnTransformChanged()
	{
		AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);

		// To improve performance set skinning dirty now, so the bone nodes will not redundantly signal transform changes back
		modelDrawable->animatedModelFlags |= AMF_SKINNING_DIRTY;

		// If have other children than the root bone, dirty the hierarchy normally. Otherwise optimize
		if (children.size() > 1) {
			SpatialNode::OnTransformChanged();
		} else {
			modelDrawable->SetBoneTransformsDirty();
			modelDrawable->SetFlag(Drawable::FLAG_WORLD_TRANSFORM_DIRTY, true);
			SetFlag(FLAG_WORLDTRANSFORMDIRTY, true);
		}

		modelDrawable->SetFlag(Drawable::FLAG_BOUNDING_BOX_DIRTY, true);
		if (octree && modelDrawable->octant && !modelDrawable->TestFlag(Drawable::FLAG_OCTREE_REINSERT_QUEUED)) {
			octree->QueueUpdate(modelDrawable);
		}
	}
}
