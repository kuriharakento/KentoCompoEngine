#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/GraphicsTypes.h"

namespace KCE
{
constexpr uint32_t kMaxAnimationPoseLayers = 4;

/** @brief ポーズを構成する1枚のアニメーションレイヤー */
struct AnimationPoseLayer
{
	const AnimationClip* clip = nullptr;
	float sampleTime = 0.0f;
	float weight = 0.0f;
};

/** @brief 共有ポーズを探す鍵。レイヤーを足しても鍵の形を変えずに済む */
struct AnimationPoseKey
{
	const Skeleton* skeleton = nullptr;
	std::array<AnimationPoseLayer, kMaxAnimationPoseLayers> layers{};
	uint32_t layerCount = 0;
};

/** @brief 1時点のスケルトン計算結果と、計算に使う再利用領域 */
struct AnimationPose
{
	std::vector<Matrix4x4> localBoneMatrices;
	std::vector<Matrix4x4> globalBoneMatrices;
	std::vector<Matrix4x4> finalBoneMatrices;
	std::vector<uint8_t> computedBones;
};

/**
 * @brief 同じスケルトン・クリップ・時刻のポーズを1フレーム内で共有する。
 * @details メインスレッド専用。並列更新へ移すときは同期方法を見直す。
 */
class AnimationPoseCache
{
public:
	/** @brief シングルトンを取得する */
	static AnimationPoseCache& GetInstance();

	/**
	 * @brief 同じフレームの同じ鍵を使い回し、無ければ計算する。
	 * @return 次のフレームでスロットが再利用されるまで有効なポーズ
	 */
	const AnimationPose& FindOrCreate(const AnimationPoseKey& key, uint64_t frameNumber);

	/** @brief キャッシュを使わず、指定領域へポーズを計算する */
	void Evaluate(const AnimationPoseKey& key, AnimationPose& destination) const;

	/** @brief 現在フレームに実行したポーズ計算の回数 */
	uint32_t GetCalculationCount() const { return calculationCount_; }
	/** @brief 現在フレームに使っているポーズ数 */
	uint32_t GetActivePoseCount() const { return activePoseCount_; }
	/** @brief 確保済みで再利用できるスロット数 */
	size_t GetSlotCount() const { return slots_.size(); }

private:
	struct Slot
	{
		AnimationPoseKey key;
		AnimationPose pose;
		uint64_t lastUsedFrame = 0;
	};

	AnimationPoseCache() = default;
	AnimationPoseCache(const AnimationPoseCache&) = delete;
	AnimationPoseCache& operator=(const AnimationPoseCache&) = delete;

	static bool KeysEqual(const AnimationPoseKey& lhs, const AnimationPoseKey& rhs);
	void BeginFrame(uint64_t frameNumber);

	std::vector<std::unique_ptr<Slot>> slots_;
	uint64_t currentFrame_ = 0;
	bool hasCurrentFrame_ = false;
	uint32_t calculationCount_ = 0;
	uint32_t activePoseCount_ = 0;
};
} // namespace KCE
