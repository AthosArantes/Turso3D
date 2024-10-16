#include <Turso3D/Renderer/AnimationState.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Renderer/AnimatedModel.h>
#include <Turso3D/Renderer/Animation.h>
#include <cassert>

namespace Turso3D
{
	AnimationState::AnimationState(AnimatedModelDrawable* drawable, const std::shared_ptr<Animation>& animation) :
		drawable(drawable),
		animation(animation),
		looped(false),
		weight(0.0f),
		time(0.0f),
		blendLayer(0)
	{
		assert(drawable);
		assert(animation);

		// Set default start bone (use all tracks.)
		SetStartBone(nullptr);
	}

	AnimationState::AnimationState(SpatialNode* node, const std::shared_ptr<Animation>& animation) :
		drawable(nullptr),
		rootNode(node),
		animation(animation),
		startBone(nullptr),
		looped(false),
		weight(1.0f),
		time(0.0f),
		blendLayer(0)
	{
		assert(node);
		assert(animation);

		const std::map<StringHash, AnimationTrack>& tracks = animation->Tracks();
		stateTracks.clear();

		for (auto it = tracks.begin(); it != tracks.end(); ++it) {
			if (it->second.keyFrames.empty()) {
				continue;
			}

			AnimationStateTrack stateTrack;
			stateTrack.track = &it->second;

			if (node->NameHash() == it->second.nameHash || tracks.size() == 1) {
				stateTrack.node = node;
			} else {
				Node* targetNode = node->FindChild(it->second.nameHash, true);
				if (targetNode && targetNode->TestFlag(Node::FLAG_SPATIAL)) {
					stateTrack.node = static_cast<SpatialNode*>(targetNode);
				} else {
					LOG_WARNING("Node \"{:s}\" not found for node animation \"{:s}\".", it->second.name, animation->Name());
				}
			}

			if (stateTrack.node) {
				stateTracks.push_back(stateTrack);
			}
		}
	}

	void AnimationState::SetStartBone(Bone* startBone_)
	{
		if (!drawable) {
			return;
		}

		if (!startBone_) {
			startBone_ = drawable->RootBone();
		}

		// Do not reassign if the start bone did not actually change, and we already have valid bone nodes
		if (startBone_ == startBone && !stateTracks.empty()) {
			return;
		}

		startBone = startBone_;

		const std::map<StringHash, AnimationTrack>& tracks = animation->Tracks();
		stateTracks.clear();

		for (auto it = tracks.begin(); it != tracks.end(); ++it) {
			if (it->second.keyFrames.empty()) {
				continue;
			}

			AnimationStateTrack stateTrack;
			stateTrack.track = &it->second;

			// Include those tracks that are either the start bone itself, or its children
			const StringHash& nameHash = it->second.nameHash;

			if (nameHash == startBone->NameHash()) {
				stateTrack.node = startBone;
			} else {
				Node* bone = startBone->FindChild(nameHash, true);
				if (bone->TestFlag(Node::FLAG_BONE)) {
					stateTrack.node = static_cast<SpatialNode*>(bone);
				}
			}

			if (stateTrack.node) {
				stateTracks.push_back(stateTrack);
			}
		}

		drawable->OnAnimationOrderChanged();
	}

	void AnimationState::SetLooped(bool looped_)
	{
		looped = looped_;
	}

	void AnimationState::SetWeight(float weight_)
	{
		// Weight can only be set in model mode. In node animation it is hardcoded to full
		if (!drawable) {
			return;
		}

		weight_ = Clamp(weight_, 0.0f, 1.0f);
		if (weight_ != weight) {
			weight = weight_;
			drawable->OnAnimationChanged();
		}
	}

	void AnimationState::SetTime(float time_)
	{
		time_ = Clamp(time_, 0.0f, animation->Length());
		if (time_ != time) {
			time = time_;
			if (drawable && weight > 0.0f) {
				drawable->OnAnimationChanged();
			}
		}
	}

	void AnimationState::SetBoneWeight(size_t index, float weight_, bool recursive)
	{
		if (index >= stateTracks.size()) {
			return;
		}

		weight_ = Clamp(weight_, 0.0f, 1.0f);
		if (weight_ != stateTracks[index].weight) {
			stateTracks[index].weight = weight_;
			if (drawable) {
				drawable->OnAnimationChanged();
			}
		}

		if (recursive && stateTracks[index].node) {
			for (const std::unique_ptr<Node>& child : stateTracks[index].node->Children()) {
				if (child->TestFlag(Node::FLAG_BONE)) {
					size_t trackIndex = FindTrackIndex(static_cast<Bone*>(child.get()));
					if (trackIndex < stateTracks.size()) {
						SetBoneWeight(trackIndex, weight, true);
					}
				}
			}
		}
	}

	void AnimationState::SetBoneWeight(const std::string& name, float weight_, bool recursive)
	{
		SetBoneWeight(FindTrackIndex(name), weight_, recursive);
	}

	void AnimationState::SetBoneWeight(StringHash nameHash, float weight_, bool recursive)
	{
		SetBoneWeight(FindTrackIndex(nameHash), weight_, recursive);
	}

	void AnimationState::AddWeight(float delta)
	{
		if (delta != 0.0f) {
			SetWeight(Weight() + delta);
		}
	}

	void AnimationState::AddTime(float delta)
	{
		float length = animation->Length();
		if (delta == 0.0f || length == 0.0f) {
			return;
		}

		float oldTime = time;
		float newTime = oldTime + delta;
		if (looped) {
			while (newTime >= length) {
				newTime -= length;
			}
			while (newTime < 0.0f) {
				newTime += length;
			}
		}

		SetTime(newTime);
	}

	void AnimationState::SetBlendLayer(unsigned char layer)
	{
		if (layer != blendLayer) {
			blendLayer = layer;
			if (drawable) {
				drawable->OnAnimationOrderChanged();
			}
		}
	}

	float AnimationState::BoneWeight(size_t index) const
	{
		return index < stateTracks.size() ? stateTracks[index].weight : 0.0f;
	}

	float AnimationState::BoneWeight(const std::string& name) const
	{
		return BoneWeight(FindTrackIndex(name));
	}

	float AnimationState::BoneWeight(StringHash nameHash) const
	{
		return BoneWeight(FindTrackIndex(nameHash));
	}

	size_t AnimationState::FindTrackIndex(SpatialNode* node) const
	{
		for (unsigned i = 0; i < stateTracks.size(); ++i) {
			if (stateTracks[i].node == node) {
				return i;
			}
		}
		return UINT_MAX;
	}

	size_t AnimationState::FindTrackIndex(const std::string& name) const
	{
		for (size_t i = 0; i < stateTracks.size(); ++i) {
			SpatialNode* node = stateTracks[i].node;
			if (node && node->Name() == name) {
				return i;
			}
		}
		return UINT_MAX;
	}

	size_t AnimationState::FindTrackIndex(StringHash nameHash) const
	{
		for (unsigned i = 0; i < stateTracks.size(); ++i) {
			Node* node = stateTracks[i].node;
			if (node && node->NameHash() == nameHash) {
				return i;
			}
		}
		return UINT_MAX;
	}

	float AnimationState::Length() const
	{
		return animation->Length();
	}

	void AnimationState::Apply()
	{
		if (drawable) {
			ApplyToModel();
		} else {
			ApplyToNodes();
		}
	}

	void AnimationState::ApplyToModel()
	{
		for (auto it = stateTracks.begin(); it != stateTracks.end(); ++it) {
			AnimationStateTrack& stateTrack = *it;
			const AnimationTrack* track = stateTrack.track;
			float finalWeight = weight * stateTrack.weight;
			Bone* bone = static_cast<Bone*>(stateTrack.node);

			// Do not apply if zero effective weight or the bone has animation disabled
			if (EpsilonEquals(finalWeight, 0.0f) || !bone->AnimationEnabled()) {
				continue;
			}

			track->FindKeyFrameIndex(time, stateTrack.keyFrame);
			const AnimationKeyFrame& keyFrame = track->keyFrames[stateTrack.keyFrame];

			// Check if next frame to interpolate to is valid, or if wrapping is needed (looping animation only)
			size_t nextFrame = stateTrack.keyFrame + 1;
			bool interpolate = true;
			if (nextFrame >= track->keyFrames.size()) {
				if (!looped) {
					nextFrame = stateTrack.keyFrame;
					interpolate = false;
				} else {
					nextFrame = 0;
				}
			}

			Vector3 newPosition = bone->Position();
			Quaternion newRotation = bone->Rotation();
			Vector3 newScale = bone->Scale();

			if (interpolate) {
				const AnimationKeyFrame& nextKeyFrame = track->keyFrames[nextFrame];
				float timeInterval = nextKeyFrame.time - keyFrame.time;
				if (timeInterval < 0.0f) {
					timeInterval += animation->Length();
				}
				float t = timeInterval > 0.0f ? (time - keyFrame.time) / timeInterval : 1.0f;

				if (track->channelMask & CHANNEL_POSITION) {
					newPosition = keyFrame.position.Lerp(nextKeyFrame.position, t);
				}
				if (track->channelMask & CHANNEL_ROTATION) {
					newRotation = keyFrame.rotation.Slerp(nextKeyFrame.rotation, t);
				}
				if (track->channelMask & CHANNEL_SCALE) {
					newScale = keyFrame.scale.Lerp(nextKeyFrame.scale, t);
				}
			} else {
				if (track->channelMask & CHANNEL_POSITION) {
					newPosition = keyFrame.position;
				}
				if (track->channelMask & CHANNEL_ROTATION) {
					newRotation = keyFrame.rotation;
				}
				if (track->channelMask & CHANNEL_SCALE) {
					newScale = keyFrame.scale;
				}
			}

			// If not full weight, blend
			if (weight < 1.0f) {
				if (track->channelMask & CHANNEL_POSITION) {
					newPosition = bone->Position().Lerp(newPosition, weight);
				}
				if (track->channelMask & CHANNEL_ROTATION) {
					newRotation = bone->Rotation().Slerp(newRotation, weight);
				}
				if (track->channelMask & CHANNEL_SCALE) {
					newScale = bone->Scale().Lerp(newScale, weight);
				}
			}

			bone->SetTransformSilent(newPosition, newRotation, newScale);
		}
	}

	void AnimationState::ApplyToNodes()
	{
		// When applying to a node hierarchy, can only use full weight (nothing to blend to)
		for (auto it = stateTracks.begin(); it != stateTracks.end(); ++it) {
			AnimationStateTrack& stateTrack = *it;
			const AnimationTrack* track = stateTrack.track;
			SpatialNode* node = stateTrack.node;

			track->FindKeyFrameIndex(time, stateTrack.keyFrame);
			const AnimationKeyFrame& keyFrame = track->keyFrames[stateTrack.keyFrame];

			// Check if next frame to interpolate to is valid, or if wrapping is needed (looping animation only)
			size_t nextFrame = stateTrack.keyFrame + 1;
			bool interpolate = true;
			if (nextFrame >= track->keyFrames.size()) {
				if (!looped) {
					nextFrame = stateTrack.keyFrame;
					interpolate = false;
				} else {
					nextFrame = 0;
				}
			}

			Vector3 newPosition = node->Position();
			Quaternion newRotation = node->Rotation();
			Vector3 newScale = node->Scale();

			if (interpolate) {
				const AnimationKeyFrame& nextKeyFrame = track->keyFrames[nextFrame];
				float timeInterval = nextKeyFrame.time - keyFrame.time;
				if (timeInterval < 0.0f) {
					timeInterval += animation->Length();
				}
				float t = timeInterval > 0.0f ? (time - keyFrame.time) / timeInterval : 1.0f;

				if (track->channelMask & CHANNEL_POSITION) {
					newPosition = keyFrame.position.Lerp(nextKeyFrame.position, t);
				}
				if (track->channelMask & CHANNEL_ROTATION) {
					newRotation = keyFrame.rotation.Slerp(nextKeyFrame.rotation, t);
				}
				if (track->channelMask & CHANNEL_SCALE) {
					newScale = keyFrame.scale.Lerp(nextKeyFrame.scale, t);
				}
			} else {
				if (track->channelMask & CHANNEL_POSITION) {
					newPosition = keyFrame.position;
				}
				if (track->channelMask & CHANNEL_ROTATION) {
					newRotation = keyFrame.rotation;
				}
				if (track->channelMask & CHANNEL_SCALE) {
					newScale = keyFrame.scale;
				}
			}

			node->SetTransform(newPosition, newRotation, newScale);
		}
	}
}
