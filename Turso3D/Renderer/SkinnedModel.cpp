#include <Turso3D/Core/Allocator.h>
#include <Turso3D/Graphics/UniformBuffer.h>
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Renderer/Camera.h>
#include <Turso3D/Renderer/DebugRenderer.h>
#include <Turso3D/Renderer/Model.h>
#include <Turso3D/Renderer/Octree.h>
#include <Turso3D/Renderer/SkinnedModel.h>

namespace
{
	using namespace Turso3D;

	static Allocator<SkinnedModelDrawable> drawableAllocator;
}

namespace Turso3D
{
	SkinnedModelDrawable::SkinnedModelDrawable() :
		skinFlags(0)
	{
		SetFlag(Drawable::FLAG_SKINNED_GEOMETRY | Drawable::FLAG_OCTREE_UPDATE_CALL, true);
	}

	SkinnedModelDrawable::~SkinnedModelDrawable()
	{
	}

	void SkinnedModelDrawable::OnWorldBoundingBoxUpdate() const
	{
		const std::vector<Bone*>& bones = Bones();
		if (model && !bones.empty()) {
			const Bone* root_bone = RootBone();

			// Recalculate bounding box from bones only if they moved individually
			if (skinFlags & FLAG_BONE_BOUNDING_BOX_DIRTY) {
				const std::vector<ModelBone>& modelBones = model->Bones();

				// Use a temporary bounding box for calculations in case many threads call this simultaneously
				BoundingBox tempBox;

				// Apply additional transformations if not sharing the same parent node from the root bone.
				if (owner->Parent() != root_bone->Parent()) {
					const Matrix3x4& wt = root_bone->WorldTransform();
					const Matrix3x4& wti = wt.Inverse();
					const Matrix3x4& drawable_space = wti * owner->WorldTransform();

					for (size_t i = 0; i < bones.size(); ++i) {
						if (modelBones[i].active) {
							const Matrix3x4& bone_space = wti * bones[i]->WorldTransform();
							tempBox.Merge(modelBones[i].boundingBox.Transformed(wt * (drawable_space * bone_space)));
						}
					}
				} else {
					for (size_t i = 0; i < bones.size(); ++i) {
						if (modelBones[i].active) {
							tempBox.Merge(modelBones[i].boundingBox.Transformed(bones[i]->WorldTransform()));
						}
					}
				}

				worldBoundingBox = tempBox;
				boneBoundingBox = tempBox.Transformed(WorldTransform().Inverse());
				skinFlags &= ~FLAG_BONE_BOUNDING_BOX_DIRTY;

			} else {
				worldBoundingBox = boneBoundingBox.Transformed(WorldTransform());
			}
		} else {
			Drawable::OnWorldBoundingBoxUpdate();
		}
	}

	void SkinnedModelDrawable::OnOctreeUpdate(unsigned short frameNumber)
	{
		if (skinFlags & FLAG_SKINNING_DIRTY) {
			UpdateSkinning();
		}
	}

	bool SkinnedModelDrawable::OnPrepareRender(unsigned short frameNumber, Camera* camera)
	{
		if (!StaticModelDrawable::OnPrepareRender(frameNumber, camera)) {
			return false;
		}
		PrepareForRender();
		return true;
	}

	void SkinnedModelDrawable::OnRender(ShaderProgram*, size_t)
	{
		const std::vector<Bone*>& bones = Bones();
		if (!skinMatrixBuffer || bones.empty()) {
			return;
		}

		if (skinFlags & FLAG_SKINNING_BUFFER_DIRTY) {
			skinMatrixBuffer->SetData(0, bones.size() * sizeof(Matrix3x4), skinMatrices.get());
			skinFlags &= ~FLAG_SKINNING_BUFFER_DIRTY;
		}

		Graphics::BindUniformBuffer(UB_OBJECTDATA, skinMatrixBuffer.get());
	}

	void SkinnedModelDrawable::OnRaycast(std::vector<RaycastResult>& dest, const Ray& ray, float maxDistance_)
	{
		const Bone* root_bone = RootBone();
		if (!root_bone) {
			return;
		}

		if (ray.HitDistance(WorldBoundingBox()) < maxDistance_ && model) {
			RaycastResult res;
			res.distance = M_INFINITY;

			// Perform raycast against each bone in its local space
			const std::vector<ModelBone>& modelBones = model->Bones();

			const Matrix3x4& wt = root_bone->WorldTransform();
			const Matrix3x4& wti = wt.Inverse();
			const Matrix3x4& drawable_space = wti * owner->WorldTransform();

			const std::vector<Bone*>& bones = Bones();
			for (size_t i = 0; i < bones.size(); ++i) {
				if (!modelBones[i].active) {
					continue;
				}

				Matrix3x4 transform;
				if (owner->Parent() != root_bone->Parent()) {
					const Matrix3x4& bone_space = wti * bones[i]->WorldTransform();
					transform = wt * (drawable_space * bone_space);
				} else {
					transform = bones[i]->WorldTransform();
				}

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

	void SkinnedModelDrawable::OnRenderDebug(DebugRenderer* debug)
	{
		debug->AddBoundingBox(WorldBoundingBox(), Color::GREEN(), false);

		// Do not render the bone hierarchy as it'll be done by the owner.
		const Bone* root_bone = RootBone();
		if (!root_bone || root_bone->Parent() != owner) {
			return;
		}

		const std::vector<Bone*>& bones = Bones();
		for (size_t i = 0; i < bones.size(); ++i) {
			Bone* bone = bones[i];
			// Skip the root bone(s), as it has no sensible connection point
			if (bone->Parent() == root_bone) {
				continue;
			}
			debug->AddLine(bone->WorldPosition(), bone->SpatialParent()->WorldPosition(), Color::WHITE(), false);
		}
	}

	void SkinnedModelDrawable::UpdateSkinning()
	{
		const Bone* root_bone = RootBone();
		const std::vector<Bone*>& bones = Bones();
		const std::vector<ModelBone>& modelBones = model->Bones();

		// Apply additional transformations if not sharing the same parent node from the root bone.
		if (owner->Parent() != root_bone->Parent()) {
			const Matrix3x4& wt = root_bone->WorldTransform();
			const Matrix3x4& wti = wt.Inverse();
			const Matrix3x4& drawable_space = wti * owner->WorldTransform();

			for (size_t i = 0; i < bones.size(); ++i) {
				const Matrix3x4& bone_space = wti * bones[i]->WorldTransform();
				skinMatrices[i] = (wt * (drawable_space * bone_space)) * modelBones[i].offsetMatrix;
			}
		} else {
			for (size_t i = 0; i < bones.size(); ++i) {
				skinMatrices[i] = bones[i]->WorldTransform() * modelBones[i].offsetMatrix;
			}
		}

		skinFlags &= ~FLAG_SKINNING_DIRTY;
		skinFlags |= FLAG_SKINNING_BUFFER_DIRTY;
	}

	const std::vector<Bone*>& SkinnedModelDrawable::Bones() const
	{
		return static_cast<SkinnedModel*>(owner)->Bones();
	}

	Bone* SkinnedModelDrawable::RootBone() const
	{
		return static_cast<SkinnedModel*>(owner)->RootBone();
	}

	void SkinnedModelDrawable::PrepareForRender()
	{
		if (skinFlags & FLAG_SKINNING_DIRTY) {
			UpdateSkinning();
		}
	}

	// ==========================================================================================
	Bone::Bone() :
		listener(nullptr),
		animationEnabled(true),
		numChildBones(0)
	{
		SetFlag(FLAG_BONE, true);
	}

	Bone::~Bone()
	{
	}

	void Bone::SetListener(BoneListener* newListener)
	{
		listener = newListener;
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

	void Bone::SetTransformDirty()
	{
		// If bone has only other bones as children, just set its world transform dirty without going through the hierarchy.
		// The whole hierarchy will be eventually updated
		if (NumChildren() == NumChildBones()) {
			SetFlag(Node::FLAG_WORLDTRANSFORMDIRTY, true);
		} else {
			OnTransformChanged();
		}
	}

	void Bone::OnTransformChanged()
	{
		// Improve performance by temporarily disabling bone transforms callbacks.
		bool notify = (listener && listener->boneListening);
		if (notify) {
			listener->boneListening = false;
		}

		SpatialNode::OnTransformChanged();

		if (notify) {
			listener->boneListening = true;
			listener->OnBoneTransformChanged(this);
		}
	}

	// ==========================================================================================
	SkinnedModel::SkinnedModel(Drawable* drawable) :
		StaticModel(drawable),
		rootBone(nullptr)
	{
		boneListening = true;
	}

	SkinnedModel::SkinnedModel() :
		StaticModel(drawableAllocator.Allocate()),
		rootBone(nullptr)
	{
		boneListening = true;
	}

	SkinnedModel::~SkinnedModel()
	{
		if (drawable) {
			RemoveFromOctree();
			drawableAllocator.Free(static_cast<SkinnedModelDrawable*>(drawable));
			drawable = nullptr;
		}
	}

	void SkinnedModel::SetupBones(Bone* root)
	{
		SkinnedModelDrawable* drawable = GetDrawable();

		if (!drawable->model) {
			return;
		}

		if (!root) {
			root = CreateChild<Bone>();
			root->SetListener(this);
		}
		rootBone = root;

		const std::vector<ModelBone>& modelBones = drawable->model->Bones();
		bones.resize(modelBones.size());

		// Create matrices buffer
		drawable->skinMatrices = std::make_unique<Matrix3x4[]>(bones.size());
		drawable->skinMatrixBuffer = std::make_unique<UniformBuffer>();
		drawable->skinMatrixBuffer->Define(USAGE_DYNAMIC, bones.size() * sizeof(Matrix3x4));

		for (size_t i = 0; i < modelBones.size(); ++i) {
			const ModelBone& modelBone = modelBones[i];

			Node* existingBone = rootBone->FindChild(modelBone.nameHash, true);
			if (existingBone && existingBone->TestFlag(Node::FLAG_BONE)) {
				bones[i] = static_cast<Bone*>(existingBone);
			} else {
				bones[i] = new Bone();
				bones[i]->SetListener(root->Listener());
				bones[i]->SetName(modelBone.name);
				bones[i]->SetTransform(modelBone.position, modelBone.rotation, modelBone.scale);
			}
		}

		// Loop through bones again to set the correct parents
		for (size_t i = 0; i < modelBones.size(); ++i) {
			if (bones[i]->Parent()) {
				continue;
			}

			const ModelBone& desc = modelBones[i];
			if (desc.parentIndex == i) {
				rootBone->AddChild(std::unique_ptr<Bone>(bones[i]));
			} else {
				bones[desc.parentIndex]->AddChild(std::unique_ptr<Bone>(bones[i]));
			}
		}

		// Count child bones now for optimized transform dirtying
		for (size_t i = 0; i < bones.size(); ++i) {
			bones[i]->CountChildBones();
		}
	}

	void SkinnedModel::SetSkinningDirty()
	{
		SkinnedModelDrawable* drawable = GetDrawable();

		constexpr unsigned dirtyFlags = (SkinnedModelDrawable::FLAG_SKINNING_DIRTY | SkinnedModelDrawable::FLAG_BONE_BOUNDING_BOX_DIRTY);
		if ((drawable->skinFlags & dirtyFlags) == dirtyFlags) {
			return;
		}
		drawable->skinFlags |= dirtyFlags;

		drawable->SetFlag(Drawable::FLAG_BOUNDING_BOX_DIRTY, true);
		if (octree && drawable->octant && !TestFlag(Drawable::FLAG_OCTREE_REINSERT_QUEUED)) {
			octree->QueueUpdate(drawable);
		}
	}

	void SkinnedModel::SetBonesDirty()
	{
		// Prevent skinned model that didn't create the root bone change it's properties.
		if (!rootBone || rootBone->Parent() != this) {
			return;
		}

		// Improve performance by temporarily disabling bone transforms callbacks.
		boneListening = false;
		{
			rootBone->SetTransformDirty();
			for (size_t i = 0; i < bones.size(); ++i) {
				bones[i]->SetTransformDirty();
			}
		}
		boneListening = true;
		OnBoneTransformChanged(rootBone);
	}

	void SkinnedModel::OnTransformChanged()
	{
		SkinnedModelDrawable* drawable = GetDrawable();

		// If have other children than the root bone, dirty the hierarchy normally. Otherwise optimize
		if (children.size() > 1) {
			SpatialNode::OnTransformChanged();

		} else {
			SetBonesDirty();

			drawable->SetFlag(Drawable::FLAG_WORLD_TRANSFORM_DIRTY, true);
			SetFlag(Node::FLAG_WORLDTRANSFORMDIRTY, true);
		}
	}

	void SkinnedModel::OnBoneTransformChanged(Bone* bone)
	{
		SetSkinningDirty();
	}
}
