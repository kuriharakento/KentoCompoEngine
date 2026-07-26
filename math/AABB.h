#pragma once
#include "Vector3.h"

namespace KCE
{
/** @brief 半分のスケール係数 */
constexpr float kHalfScale = 0.5f;

/**
 * @brief 軸平行境界ボックス（Axis-Aligned Bounding Box）
 * 
 * 3D空間において、各軸に平行な辺を持つ直方体の境界ボックスを表現する構造体。
 * 衝突判定やカリングなどに使用される基本的なバウンディングボリューム。
 */
struct AABB
{
	Vector3 min_; // 境界ボックスの最小座標
	Vector3 max_; // 境界ボックスの最大座標

	/**
	 * @brief デフォルトコンストラクタ
	 */
	AABB() : min_(), max_() {}

	/**
	 * @brief 最小・最大座標を指定するコンストラクタ
	 * @param min 境界ボックスの最小座標
	 * @param max 境界ボックスの最大座標
	 */
	AABB(const Vector3& min, const Vector3& max) : min_(min), max_(max) {}

	/**
	 * @brief 境界ボックスの中心座標を取得
	 * @return 中心座標（min + max）/ 2
	 */
	Vector3 GetCenter() const
	{
		return (min_ + max_) * kHalfScale;
	}

	/**
	 * @brief 境界ボックスのサイズを取得
	 * @return サイズ（max - min）
	 */
	Vector3 GetSize() const
	{
		return max_ - min_;
	}

	/**
	 * @brief 境界ボックスの半サイズを取得
	 * @return 半サイズ（max - min）/ 2
	 */
	Vector3 GetHalfSize() const
	{
		return (max_ - min_) * kHalfScale;
	}
};
} // namespace KCE
