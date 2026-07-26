#pragma once
#include <vector>
#include <string>

#include "base/GraphicsTypes.h"
#include "math/Quaternion.h"

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

	/**
	 * @brief 最終ボーン行列の取得（GPU用）
	 */
	const std::vector<Matrix4x4>& GetFinalBoneMatrices() const { return finalBoneMatrices_; }

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
	void CalculateBoneTransforms();

	/**
	 * @brief 位置キーフレームの補間
	 */
	Vector3 InterpolatePosition(const std::vector<AnimationKey<Vector3>>& keys, float time) const;

	/**
	 * @brief 回転キーフレームの補間
	 */
	Quaternion InterpolateRotation(const std::vector<AnimationKey<Quaternion>>& keys, float time) const;

	/**
	 * @brief スケールキーフレームの補間
	 */
	Vector3 InterpolateScale(const std::vector<AnimationKey<Vector3>>& keys, float time) const;

	/**
	 * @brief キーフレームのインデックスを検索
	 */
	template <typename T>
	uint32_t FindKeyIndex(const std::vector<AnimationKey<T>>& keys, float time) const;

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

	// ボーンごとのローカル変換行列
	std::vector<Matrix4x4> localBoneMatrices_;

	// ボーンごとのグローバル変換行列
	std::vector<Matrix4x4> globalBoneMatrices_;

	// 最終ボーン行列（GPU送信用: offsetMatrix × globalTransform）
	std::vector<Matrix4x4> finalBoneMatrices_;

	// ルートのワールド行列
	Matrix4x4 worldMatrix_;
};
} // namespace KCE
