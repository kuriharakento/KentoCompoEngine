#pragma once
#include "application/gameObject/base/GameObject.h"
#include "math/OBB.h"
#include "math/Vector3.h"

class GameObject;

/**
 * @brief 衝突情報を格納する構造体
 */
struct CollisionInfo
{
	// 衝突しているかどうか
	bool    isColliding = false;
	
	// MTV（最小変位ベクトル）の方向軸
	Vector3 mtvAxis{ 0, 1, 0 };
	
	// MTVのめり込み深度
	float   mtvDepth = FLT_MAX;
};

/**
 * @brief 衝突判定のユーティリティ関数を提供する名前空間
 * 
 * MTV（Minimum Translation Vector: 最小変位ベクトル）の計算や
 * めり込み解決などの高度な衝突処理機能を提供します。
 */
namespace collisionUtils
{
	/**
	 * @brief OBB同士のMTV付き衝突判定
	 * 
	 * 分離軸定理（SAT）を使用してOBB同士の衝突を判定し、
	 * 衝突時には最小変位ベクトル（MTV）を計算します。
	 * MTVは物体を押し出すために必要な最小の移動量を表します。
	 * 
	 * アルゴリズム:
	 * 1. 15の分離軸（各OBBの3軸 + 外積9軸）で判定
	 * 2. 各軸での投影を計算してめり込み深度を算出
	 * 3. 最小のめり込み深度を持つ軸をMTVとして採用
	 * 
	 * @param obbA 判定対象のOBB A
	 * @param obbB 判定対象のOBB B
	 * @param mtv [out] 計算されたMTV（最小変位ベクトル）
	 * @return 衝突している場合true
	 */
	bool CheckOBBvsOBBMTV(const OBB& obbA, const OBB& obbB, Vector3& mtv);
	
	/**
	 * @brief めり込みを解決する
	 * 
	 * 2つのGameObject間のめり込みを解決し、位置を調整します。
	 * 
	 * @param self 解決対象のGameObject（自分）
	 * @param other 解決対象のGameObject（相手）
	 */
	void ResolvePenetration(GameObject* self, GameObject* other);
}
