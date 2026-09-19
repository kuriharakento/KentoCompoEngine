#pragma once
#include <vector>
#include <string>

#include "animation/AnimationPoseCache.h"
#include "base/GraphicsTypes.h"

namespace KCE
{
/**
 * @brief アニメーター
 * @details スケルタルアニメーションの再生を管理する。
 *          キーフレーム補間とボーン行列の計算を行う。
 */
class Animator
{
public:
	Animator() = default;

	/**
	 * @brief 初期化
	 * @param skeleton スケルトンへの参照
	 */
	void Initialize(const Skeleton* skeleton);

	/**
	 * @brief 更新
	 * @param deltaTime フレーム時間（秒）
	 */
	void Update(float deltaTime);

	/**
	 * @brief アニメーションの再生
	 * @param clip 再生するアニメーションクリップ
	 * @param loop ループ再生するか
	 */
	void PlayAnimation(const AnimationClip* clip, bool loop = true);

	/**
	 * @brief アニメーションの停止
	 */
	void StopAnimation();

	/**
	 * @brief アニメーションが再生中か
	 */
	bool IsPlaying() const { return isPlaying_; }

	/**
	 * @brief 現在の再生時間を取得
	 */
	float GetCurrentTime() const { return currentTime_; }

	/**
	 * @brief 現在の再生時間を設定
	 */
	void SetCurrentTime(float time) { currentTime_ = time; }

	/**
	 * @brief 再生速度を設定
	 */
	void SetPlaybackSpeed(float speed) { playbackSpeed_ = speed; }

	/** @brief 同じポーズの共有を使うか設定する（既定は有効） */
	void SetPoseSharingEnabled(bool enabled);
	/** @brief 同じポーズの共有を使うか */
	bool IsPoseSharingEnabled() const { return poseSharingEnabled_; }

	/**
	 * @brief 共有ポーズの時刻をまとめる幅を設定する。
	 * @param intervalSeconds 秒単位の幅。0以下なら丸めず、完全に同じ時刻だけ共有する。
	 */
	void SetPoseQuantizationInterval(float intervalSeconds);
	/** @brief 共有ポーズの時刻をまとめる幅を取得する */
	float GetPoseQuantizationInterval() const { return poseQuantizationInterval_; }

	/**
	 * @brief 最終ボーン行列の取得（GPU用）
	 * @details 共有中の参照は、この Animator を同じフレームで更新してから描画する間だけ有効。
	 */
	const std::vector<Matrix4x4>& GetFinalBoneMatrices() const;

	/**
	 * @brief 特定ボーンのワールド行列を取得（ボーンアタッチ用）
	 * @param boneName ボーン名
	 * @return ボーンのワールド変換行列
	 */
	Matrix4x4 GetBoneWorldMatrix(const std::string& boneName) const;

	/**
	 * @brief 特定ボーンのワールド行列を取得（インデックス指定）
	 * @param boneIndex ボーンインデックス
	 * @return ボーンのワールド変換行列
	 */
	Matrix4x4 GetBoneWorldMatrix(uint32_t boneIndex) const;

	/**
	 * @brief ルートのワールド行列を設定
	 * @param worldMatrix ワールド変換行列
	 */
	void SetWorldMatrix(const Matrix4x4& worldMatrix) { worldMatrix_ = worldMatrix; }

private:
	/**
	 * @brief ボーン行列の計算
	 */
	void CalculateBoneTransforms(bool allowSharing);

	/** @brief 現在のクリップと描画用時刻から共有鍵を作る */
	AnimationPoseKey MakePoseKey(bool quantizeTime) const;

	/** @brief 現在参照しているポーズ */
	const AnimationPose& GetActivePose() const;

private:
	// スケルトンへのポインタ
	const Skeleton* skeleton_ = nullptr;

	// 現在再生中のアニメーションクリップ
	const AnimationClip* currentClip_ = nullptr;

	// 現在の再生時間
	float currentTime_ = 0.0f;

	// 再生中フラグ
	bool isPlaying_ = false;

	// ループ再生フラグ
	bool isLooping_ = false;

	// 再生速度
	float playbackSpeed_ = 1.0f;

	// 共有しない場合と、停止中のポーズを保持する領域
	mutable AnimationPose localPose_;
	// キャッシュが所有するポーズ。次のフレームまでの非所有参照
	mutable const AnimationPose* sharedPose_ = nullptr;
	// sharedPose_ を受け取ったフレーム。次のフレームには枠が使い回されている前提で扱う
	mutable uint64_t sharedPoseFrame_ = 0;

	bool poseSharingEnabled_ = true;
	// 既定は30fps相当。再生時刻そのものは丸めない
	float poseQuantizationInterval_ = 1.0f / 30.0f;

	// ルートのワールド行列
	Matrix4x4 worldMatrix_;
};
} // namespace KCE
