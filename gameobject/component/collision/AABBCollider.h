#pragma once

#include "engine/gameobject/component/base/Collider.h"
#include "math/AABB.h"
#include "Math/Vector3.h"
#include "jsonEditor/JsonEditableBase.h"

namespace KCE
{
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
 */
namespace GameObjectComponent
{
	class AABBCollider : public Collider, public JsonEditableBase
	{
	public:
		/**
		 * @brief コンストラクタ
		 */
		AABBCollider();

		/**
		 * @brief デストラクタ
		 */
		~AABBCollider();

		/**
		 * @brief 毎フレームの更新処理
		 *
		 * GameObjectの位置に合わせてAABBの中心位置を更新します。
		 *
		 */
		void Update() override;

		/**
		 * @brief コライダーの種類を取得
		 * @return ColliderType::AABB
		 */
		ColliderType GetColliderType() const override { return ColliderType::AABB; }

		/**
		 * @brief ブロードフェーズ用 AABB を取得
		 */
		AABB GetBroadphaseAABB() const override { return aabb_; }

		/**
		 * @brief AABBデータを設定
		 * @param aabb 設定する AABB データ
		 */
		void SetAABB(const AABB& aabb) { aabb_ = aabb; }

		/**
		 * @brief AABBデータを取得
		 * @return 現在の AABB データ
		 */
		const AABB& GetAABB() const { return aabb_; }

	private:
		// AABBデータ（中心位置とサイズ）
		AABB aabb_;
	};
}
} // namespace KCE
