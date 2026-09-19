#include "Animator.h"

#include <algorithm>
#include <cmath>

#include "math/MatrixFunc.h"
#include "time/TimeManager.h"

namespace KCE
{
void Animator::Initialize(const Skeleton* skeleton)
{
	skeleton_ = skeleton;

	if (skeleton_)
	{
		size_t boneCount = skeleton_->bones.size();
		localPose_.localBoneMatrices.resize(boneCount);
		localPose_.globalBoneMatrices.resize(boneCount);
		localPose_.finalBoneMatrices.resize(boneCount);
		localPose_.computedBones.resize(boneCount);

		// 初期化：単位行列
		for (size_t i = 0; i < boneCount; ++i)
		{
			localPose_.localBoneMatrices[i] = MakeIdentity4x4();
			localPose_.globalBoneMatrices[i] = MakeIdentity4x4();
			localPose_.finalBoneMatrices[i] = MakeIdentity4x4();
		}
	}

	worldMatrix_ = MakeIdentity4x4();
}

void Animator::Update(float deltaTime)
{
	if (!isPlaying_ || !currentClip_ || !skeleton_)
	{
		return;
	}

	// 時間を進める
	currentTime_ += deltaTime * playbackSpeed_;

	// ループ処理
	if (currentTime_ >= currentClip_->duration)
	{
		if (isLooping_)
		{
			currentTime_ = std::fmod(currentTime_, currentClip_->duration);
		}
		else
		{
			currentTime_ = currentClip_->duration;
			isPlaying_ = false;
		}
	}

	// 終了したポーズは次のフレーム以降も残すため、Animator 自身の領域へ計算する
	CalculateBoneTransforms(isPlaying_);
}

void Animator::PlayAnimation(const AnimationClip* clip, bool loop)
{
	// 既に同じアニメーションを再生中の場合はリセットしない
	if (currentClip_ == clip && isPlaying_ && isLooping_ == loop)
	{
		return;
	}

	currentClip_ = clip;
	currentTime_ = 0.0f;
	isPlaying_ = true;
	isLooping_ = loop;

	if (currentClip_)
	{
		CalculateBoneTransforms(isPlaying_);
	}
}

void Animator::StopAnimation()
{
	isPlaying_ = false;
	currentTime_ = 0.0f;

	// 停止した瞬間に見た目もリセットする
	if (currentClip_ && skeleton_)
	{
		CalculateBoneTransforms(false);
	}
}

void Animator::SetPoseSharingEnabled(bool enabled)
{
	if (poseSharingEnabled_ == enabled)
	{
		return;
	}
	poseSharingEnabled_ = enabled;
	if (!enabled && sharedPose_)
	{
		localPose_ = *sharedPose_;
		sharedPose_ = nullptr;
	}
}

void Animator::SetPoseQuantizationInterval(float intervalSeconds)
{
	poseQuantizationInterval_ = (std::max)(0.0f, intervalSeconds);
}

const std::vector<Matrix4x4>& Animator::GetFinalBoneMatrices() const
{
	return GetActivePose().finalBoneMatrices;
}

Matrix4x4 Animator::GetBoneWorldMatrix(const std::string& boneName) const
{
	if (!skeleton_)
	{
		return MakeIdentity4x4();
	}

	int32_t boneIndex = skeleton_->GetBoneIndex(boneName);
	const std::vector<Matrix4x4>& globalBoneMatrices = GetActivePose().globalBoneMatrices;
	if (boneIndex < 0 || boneIndex >= static_cast<int32_t>(globalBoneMatrices.size()))
	{
		return MakeIdentity4x4();
	}

	return Multiply(globalBoneMatrices[boneIndex], worldMatrix_);
}

Matrix4x4 Animator::GetBoneWorldMatrix(uint32_t boneIndex) const
{
	const std::vector<Matrix4x4>& globalBoneMatrices = GetActivePose().globalBoneMatrices;
	if (boneIndex >= globalBoneMatrices.size())
	{
		return MakeIdentity4x4();
	}

	return Multiply(globalBoneMatrices[boneIndex], worldMatrix_);
}

void Animator::CalculateBoneTransforms(bool allowSharing)
{
	if (!skeleton_ || !currentClip_)
	{
		return;
	}

	AnimationPoseCache& cache = AnimationPoseCache::GetInstance();
	if (poseSharingEnabled_ && allowSharing)
	{
		const AnimationPoseKey key = MakePoseKey(true);
		const uint64_t frame = TimeManager::GetInstance().GetFrameCount();
		sharedPose_ = &cache.FindOrCreate(key, frame);
		sharedPoseFrame_ = frame;
		return;
	}
	sharedPose_ = nullptr;
	cache.Evaluate(MakePoseKey(false), localPose_);
}

AnimationPoseKey Animator::MakePoseKey(bool quantizeTime) const
{
	float sampleTime = currentTime_;
	if (quantizeTime && poseQuantizationInterval_ > 0.0f)
	{
		sampleTime = std::round(sampleTime / poseQuantizationInterval_) * poseQuantizationInterval_;
	}
	AnimationPoseKey key;
	key.skeleton = skeleton_;
	key.layerCount = 1;
	key.layers[0] = { currentClip_, sampleTime, 1.0f };
	return key;
}

const AnimationPose& Animator::GetActivePose() const
{
	if (!sharedPose_)
	{
		return localPose_;
	}
	// 更新されなかったフレームに描かれると、共有の枠は別のポーズに使い回されているかもしれない。
	// そのときは自分の領域へ計算し直して、以降はそれを使う
	if (sharedPoseFrame_ != TimeManager::GetInstance().GetFrameCount())
	{
		AnimationPoseCache::GetInstance().Evaluate(MakePoseKey(false), localPose_);
		sharedPose_ = nullptr;
		return localPose_;
	}
	return *sharedPose_;
}
} // namespace KCE
