#pragma once

#include "application/GameObject/component/base/ICollisionComponent.h"
#include "math/AABB.h"
#include "Math/Vector3.h"

/**
 * @brief 軸平行境界ボックス（AABB）による衝突判定コンポーネント
 * 
 * 軸に平行な矩形領域での衝突判定を行います。
 * 回転を考慮しないため高速ですが、回転したオブジェクトには不向きです。
 * 
 * 用途:
 * - 回転しないオブジェクト（壁、床、箱など）
 * - 大まかな衝突判定（広域フェーズ）
 * 
 * @note 2D/3Dモード両方に対応しています
 */
class AABBColliderComponent : public ICollisionComponent
{
public:
	/**
	 * @brief コンストラクタ
	 * @param owner このコンポーネントを所有するGameObject
	 */
	AABBColliderComponent(GameObject* owner);
	
	/**
	 * @brief デストラクタ
	 */
	~AABBColliderComponent();

	/**
	 * @brief 毎フレームの更新処理
	 * 
	 * GameObjectの位置に合わせてAABBの中心位置を更新します。
	 * 
	 * @param owner このコンポーネントを所有するGameObject
	 */
	void Update(GameObject* owner) override;
	
	/**
	 * @brief コライダーの種類を取得
	 * @return ColliderType::AABB
	 */
	ColliderType GetColliderType() const override { return ColliderType::AABB; }
	
	/**
	 * @brief AABBデータを設定
	 * @param aabb 設定するAABBデータ
	 */
	void SetAABB(const AABB& aabb) { aabb_ = aabb; }
	
	/**
	 * @brief AABBデータを取得
	 * @return 現在のAABBデータ
	 */
	const AABB& GetAABB() const { return aabb_; }

private:
	// AABBデータ（中心位置とサイズ）
	AABB aabb_;
};
