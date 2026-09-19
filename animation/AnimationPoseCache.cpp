#include "animation/AnimationPoseCache.h"

#include <algorithm>

#include "math/MatrixFunc.h"
#include "math/Quaternion.h"

namespace KCE
{
namespace
{
template <typename T>
uint32_t FindKeyIndex(const std::vector<AnimationKey<T>>& keys, float time)
{
	for (uint32_t i = 0; i + 1 < keys.size(); ++i)
	{
		if (time < keys[i + 1].time)
		{
			return i;
		}
	}
	return static_cast<uint32_t>(keys.size() - 2);
}

Vector3 InterpolateVector(const std::vector<AnimationKey<Vector3>>& keys, float time)
{
	if (keys.size() == 1)
	{
		return keys[0].value;
	}
	const uint32_t index = FindKeyIndex(keys, time);
	const uint32_t nextIndex = index + 1;
	if (nextIndex >= keys.size())
	{
		return keys.back().value;
	}
	const float duration = keys[nextIndex].time - keys[index].time;
	const float factor = (time - keys[index].time) / duration;
	const Vector3& start = keys[index].value;
	const Vector3& end = keys[nextIndex].value;
	return {
		start.x + factor * (end.x - start.x),
		start.y + factor * (end.y - start.y),
		start.z + factor * (end.z - start.z),
	};
}

Quaternion InterpolateRotation(const std::vector<AnimationKey<Quaternion>>& keys, float time)
{
	if (keys.size() == 1)
	{
		return keys[0].value;
	}
	const uint32_t index = FindKeyIndex(keys, time);
	const uint32_t nextIndex = index + 1;
	if (nextIndex >= keys.size())
	{
		return keys.back().value;
	}
	const float duration = keys[nextIndex].time - keys[index].time;
	const float factor = (time - keys[index].time) / duration;
	return Quaternion::Slerp(keys[index].value, keys[nextIndex].value, factor);
}
} // namespace

AnimationPoseCache& AnimationPoseCache::GetInstance()
{
	static AnimationPoseCache instance;
	return instance;
}

const AnimationPose& AnimationPoseCache::FindOrCreate(const AnimationPoseKey& key, uint64_t frameNumber)
{
	BeginFrame(frameNumber);
	Slot* freeSlot = nullptr;
	for (const std::unique_ptr<Slot>& slot : slots_)
	{
		if (slot->lastUsedFrame == currentFrame_)
		{
			if (KeysEqual(slot->key, key))
			{
				return slot->pose;
			}
		}
		else if (!freeSlot)
		{
			freeSlot = slot.get();
		}
	}

	if (!freeSlot)
	{
		slots_.push_back(std::make_unique<Slot>());
		freeSlot = slots_.back().get();
	}
	freeSlot->key = key;
	freeSlot->lastUsedFrame = currentFrame_;
	Evaluate(key, freeSlot->pose);
	++calculationCount_;
	++activePoseCount_;
	return freeSlot->pose;
}

void AnimationPoseCache::Evaluate(const AnimationPoseKey& key, AnimationPose& destination) const
{
	if (!key.skeleton || key.layerCount == 0 || !key.layers[0].clip)
	{
		return;
	}

	const Skeleton& skeleton = *key.skeleton;
	const AnimationPoseLayer& layer = key.layers[0];
	const size_t boneCount = skeleton.bones.size();
	destination.localBoneMatrices.resize(boneCount);
	destination.globalBoneMatrices.resize(boneCount);
	destination.finalBoneMatrices.resize(boneCount);
	destination.computedBones.resize(boneCount);
	std::fill(destination.computedBones.begin(), destination.computedBones.end(), uint8_t{ 0 });

	for (size_t i = 0; i < boneCount; ++i)
	{
		destination.localBoneMatrices[i] = skeleton.bones[i].defaultLocalTransform;
	}

	for (const AnimationChannel& channel : layer.clip->channels)
	{
		if (channel.boneIndex < 0 || channel.boneIndex >= static_cast<int32_t>(boneCount))
		{
			continue;
		}
		Vector3 position = { 0.0f, 0.0f, 0.0f };
		Quaternion rotation = Quaternion::Identity();
		Vector3 scale = { 1.0f, 1.0f, 1.0f };
		if (!channel.positionKeys.empty())
		{
			position = InterpolateVector(channel.positionKeys, layer.sampleTime);
		}
		if (!channel.rotationKeys.empty())
		{
			rotation = InterpolateRotation(channel.rotationKeys, layer.sampleTime);
		}
		if (!channel.scaleKeys.empty())
		{
			scale = InterpolateVector(channel.scaleKeys, layer.sampleTime);
		}
		destination.localBoneMatrices[channel.boneIndex] = Multiply(
			Multiply(MakeScaleMatrix(scale), rotation.ToMatrix()), MakeTranslateMatrix(position));
	}

	auto computeGlobalTransform = [&](auto&& self, size_t boneIndex) -> void
	{
		if (destination.computedBones[boneIndex] != 0)
		{
			return;
		}
		const BoneInfo& bone = skeleton.bones[boneIndex];
		if (bone.parentIndex >= 0 && bone.parentIndex < static_cast<int32_t>(boneCount))
		{
			self(self, static_cast<size_t>(bone.parentIndex));
			destination.globalBoneMatrices[boneIndex] = Multiply(
				destination.localBoneMatrices[boneIndex], destination.globalBoneMatrices[bone.parentIndex]);
		}
		else
		{
			destination.globalBoneMatrices[boneIndex] = Multiply(
				destination.localBoneMatrices[boneIndex], skeleton.armatureTransform);
		}
		destination.computedBones[boneIndex] = 1;
	};

	for (size_t i = 0; i < boneCount; ++i)
	{
		computeGlobalTransform(computeGlobalTransform, i);
		destination.finalBoneMatrices[i] = Multiply(skeleton.bones[i].offsetMatrix, destination.globalBoneMatrices[i]);
	}
}

bool AnimationPoseCache::KeysEqual(const AnimationPoseKey& lhs, const AnimationPoseKey& rhs)
{
	if (lhs.skeleton != rhs.skeleton || lhs.layerCount != rhs.layerCount)
	{
		return false;
	}
	for (uint32_t i = 0; i < lhs.layerCount; ++i)
	{
		if (lhs.layers[i].clip != rhs.layers[i].clip ||
			lhs.layers[i].sampleTime != rhs.layers[i].sampleTime ||
			lhs.layers[i].weight != rhs.layers[i].weight)
		{
			return false;
		}
	}
	return true;
}

void AnimationPoseCache::BeginFrame(uint64_t frameNumber)
{
	if (hasCurrentFrame_ && currentFrame_ == frameNumber)
	{
		return;
	}
	currentFrame_ = frameNumber;
	hasCurrentFrame_ = true;
	calculationCount_ = 0;
	activePoseCount_ = 0;
}
} // namespace KCE
