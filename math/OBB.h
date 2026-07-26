#pragma once
#include "MatrixFunc.h"
#include "math/Vector3.h"

namespace KCE
{
/**
 * @brief 有向境界ボックス（Oriented Bounding Box）
 * 
 * 3D空間において、任意の向きを持つ直方体の境界ボックスを表現する構造体。
 * AABBよりも密接にオブジェクトを囲むことができ、回転するオブジェクトの
 * 衝突判定に適している。
 */
struct OBB
{
	Vector3 center;		// 中心座標
	Matrix4x4 rotate;	// 回転行列（各軸の方向を定義）
	Vector3 size;		// サイズ（各軸方向の半径）

	/**
	 * @brief デフォルトコンストラクタ
	 */
	OBB() : center(), rotate(MakeIdentity4x4()), size() {}
};
} // namespace KCE
