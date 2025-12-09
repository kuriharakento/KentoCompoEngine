#include "Animator.h"

#include <functional>
#include "math/MatrixFunc.h"

void Animator::Initialize(const Skeleton* skeleton)
{
	skeleton_ = skeleton;

	if (skeleton_)
	{
		size_t boneCount = skeleton_->bones.size();
		localBoneMatrices_.resize(boneCount);
		globalBoneMatrices_.resize(boneCount);
		finalBoneMatrices_.resize(boneCount);

		// 初期化：単位行列
		for (size_t i = 0; i < boneCount; ++i)
		{
			localBoneMatrices_[i] = MakeIdentity4x4();
			globalBoneMatrices_[i] = MakeIdentity4x4();
			finalBoneMatrices_[i] = MakeIdentity4x4();
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

	// ボーン行列を計算
	CalculateBoneTransforms();
}

void Animator::PlayAnimation(const AnimationClip* clip, bool loop)
{
	currentClip_ = clip;
	currentTime_ = 0.0f;
	isPlaying_ = true;
	isLooping_ = loop;

	if (currentClip_)
	{
		CalculateBoneTransforms();
	}
}

void Animator::StopAnimation()
{
	isPlaying_ = false;
}

Matrix4x4 Animator::GetBoneWorldMatrix(const std::string& boneName) const
{
	if (!skeleton_)
	{
		return MakeIdentity4x4();
	}

	int32_t boneIndex = skeleton_->GetBoneIndex(boneName);
	if (boneIndex < 0 || boneIndex >= static_cast<int32_t>(globalBoneMatrices_.size()))
	{
		return MakeIdentity4x4();
	}

	return Multiply(globalBoneMatrices_[boneIndex], worldMatrix_);
}

Matrix4x4 Animator::GetBoneWorldMatrix(uint32_t boneIndex) const
{
	if (boneIndex >= globalBoneMatrices_.size())
	{
		return MakeIdentity4x4();
	}

	return Multiply(globalBoneMatrices_[boneIndex], worldMatrix_);
}

void Animator::CalculateBoneTransforms()
{
	if (!skeleton_ || !currentClip_)
	{
		return;
	}

	// 各ボーンのローカル変換をデフォルト値で初期化
	for (size_t i = 0; i < skeleton_->bones.size(); ++i)
	{
		// アニメーションがないボーンはデフォルトローカル変換（バインドポーズ）を使用
		localBoneMatrices_[i] = skeleton_->bones[i].defaultLocalTransform;
	}

	// アニメーションチャンネルからローカル変換を設定
	for (const auto& channel : currentClip_->channels)
	{
		if (channel.boneIndex < 0 || channel.boneIndex >= static_cast<int32_t>(skeleton_->bones.size()))
		{
			continue;
		}

		// 位置、回転、スケールを補間
		Vector3 position = { 0.0f, 0.0f, 0.0f };
		Quaternion rotation = Quaternion::Identity();
		Vector3 scale = { 1.0f, 1.0f, 1.0f };

		if (!channel.positionKeys.empty())
		{
			position = InterpolatePosition(channel.positionKeys, currentTime_);
		}
		if (!channel.rotationKeys.empty())
		{
			rotation = InterpolateRotation(channel.rotationKeys, currentTime_);
		}
		if (!channel.scaleKeys.empty())
		{
			scale = InterpolateScale(channel.scaleKeys, currentTime_);
		}

		// SRT行列を合成
		Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);
		Matrix4x4 rotationMatrix = rotation.ToMatrix();
		Matrix4x4 translationMatrix = MakeTranslateMatrix(position);

		localBoneMatrices_[channel.boneIndex] = Multiply(Multiply(scaleMatrix, rotationMatrix), translationMatrix);
	}

	// グローバル変換を計算（親→子の順で）
	// ボーンリストが親子順に並んでいない可能性があるため、再帰的に計算
	std::vector<bool> computed(skeleton_->bones.size(), false);
	
	std::function<void(size_t)> computeGlobalTransform = [&](size_t boneIndex)
	{
		if (computed[boneIndex]) return;
		
		const BoneInfo& bone = skeleton_->bones[boneIndex];
		
		if (bone.parentIndex >= 0 && bone.parentIndex < static_cast<int32_t>(skeleton_->bones.size()))
		{
			// 親を先に計算
			computeGlobalTransform(bone.parentIndex);
			globalBoneMatrices_[boneIndex] = Multiply(localBoneMatrices_[boneIndex], globalBoneMatrices_[bone.parentIndex]);
		}
		else
		{
			// ルートボーンにはArmatureのトランスフォームを適用
			globalBoneMatrices_[boneIndex] = Multiply(localBoneMatrices_[boneIndex], skeleton_->armatureTransform);
		}
		
		computed[boneIndex] = true;
	};
	
	for (size_t i = 0; i < skeleton_->bones.size(); ++i)
	{
		computeGlobalTransform(i);
	}

	// 最終ボーン行列を計算（オフセット行列を適用）
	// DirectXの行ベクトル規約: v' = v * M1 * M2 (左から右へ適用)
	// skinMatrix = offsetMatrix * globalBoneMatrix
	// → v * offsetMatrix で頂点をボーンローカル空間へ
	// → その結果 * globalBoneMatrix でワールド空間へ
	for (size_t i = 0; i < skeleton_->bones.size(); ++i)
	{
		finalBoneMatrices_[i] = Multiply(skeleton_->bones[i].offsetMatrix, globalBoneMatrices_[i]);
	}
}

Vector3 Animator::InterpolatePosition(const std::vector<AnimationKey<Vector3>>& keys, float time) const
{
	if (keys.size() == 1)
	{
		return keys[0].value;
	}

	uint32_t index = FindKeyIndex(keys, time);
	uint32_t nextIndex = index + 1;

	if (nextIndex >= keys.size())
	{
		return keys.back().value;
	}

	float deltaTime = keys[nextIndex].time - keys[index].time;
	float factor = (time - keys[index].time) / deltaTime;

	const Vector3& start = keys[index].value;
	const Vector3& end = keys[nextIndex].value;

	return Vector3{
		start.x + factor * (end.x - start.x),
		start.y + factor * (end.y - start.y),
		start.z + factor * (end.z - start.z)
	};
}

Quaternion Animator::InterpolateRotation(const std::vector<AnimationKey<Quaternion>>& keys, float time) const
{
	if (keys.size() == 1)
	{
		return keys[0].value;
	}

	uint32_t index = FindKeyIndex(keys, time);
	uint32_t nextIndex = index + 1;

	if (nextIndex >= keys.size())
	{
		return keys.back().value;
	}

	float deltaTime = keys[nextIndex].time - keys[index].time;
	float factor = (time - keys[index].time) / deltaTime;

	return Quaternion::Slerp(keys[index].value, keys[nextIndex].value, factor);
}

Vector3 Animator::InterpolateScale(const std::vector<AnimationKey<Vector3>>& keys, float time) const
{
	if (keys.size() == 1)
	{
		return keys[0].value;
	}

	uint32_t index = FindKeyIndex(keys, time);
	uint32_t nextIndex = index + 1;

	if (nextIndex >= keys.size())
	{
		return keys.back().value;
	}

	float deltaTime = keys[nextIndex].time - keys[index].time;
	float factor = (time - keys[index].time) / deltaTime;

	const Vector3& start = keys[index].value;
	const Vector3& end = keys[nextIndex].value;

	return Vector3{
		start.x + factor * (end.x - start.x),
		start.y + factor * (end.y - start.y),
		start.z + factor * (end.z - start.z)
	};
}

template <typename T>
uint32_t Animator::FindKeyIndex(const std::vector<AnimationKey<T>>& keys, float time) const
{
	for (uint32_t i = 0; i < keys.size() - 1; ++i)
	{
		if (time < keys[i + 1].time)
		{
			return i;
		}
	}
	return static_cast<uint32_t>(keys.size() - 2);
}

// テンプレートの明示的インスタンス化
template uint32_t Animator::FindKeyIndex<Vector3>(const std::vector<AnimationKey<Vector3>>& keys, float time) const;
template uint32_t Animator::FindKeyIndex<Quaternion>(const std::vector<AnimationKey<Quaternion>>& keys, float time) const;
