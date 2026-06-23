#pragma once
#include "engine/gameobject/component/base/ICollisionComponent.h"
#include "math/OBB.h"
#include "jsonEditor/JsonEditableBase.h"

/**
 * @brief 方向付き境界ボックス（OBB）による衝突判定コンポーネント
 * 
 * 任意の回転を持つ矩形領域での衝突判定を行います。
 * 回転したオブジェクトに正確にフィットしますが、AABBより計算コストが高くなります。
 * 
 * 判定アルゴリズム:
 * - 分離軸定理（SAT: Separating Axis Theorem）を使用
 * - 15の分離軸（各OBBの3軸 + 外積9軸）で判定
 * 
 * 用途:
 * - 回転するオブジェクト（車、飛行機、回転する障害物など）
 * - 正確な衝突判定が必要な場合
 * 
 * @note 2D/3Dモード両方に対応しています
 */
namespace GameObjectComponent
{
	class OBBColliderComponent : public ICollisionComponent, public JsonEditableBase
	{
	public:
		/**
		 * @brief コンストラクタ
		 * @param owner このコンポーネントを所有するGameObject
		 */
		OBBColliderComponent(::GameObject* owner);
		
		/**
		 * @brief デストラクタ
		 */
		~OBBColliderComponent() override;

		/**
		 * @brief 毎フレームの更新処理
		 * 
		 * GameObjectの位置と回転に合わせてOBBを更新します。
		 * 
		 * @param owner このコンポーネントを所有するGameObject
		 */
		void Update(::GameObject* owner) override;
		
		/**
		 * @brief コライダーの種類を取得
		 * @return ColliderType::OBB
		 */
		ColliderType GetColliderType() const override { return ColliderType::OBB; }

		/**
		 * @brief ブロードフェーズ用 AABB を取得
		 */
		::AABB GetBroadphaseAABB() const override
		{
			Vector3 axesX = { obb_.rotate.m[0][0], obb_.rotate.m[0][1], obb_.rotate.m[0][2] };
			Vector3 axesY = { obb_.rotate.m[1][0], obb_.rotate.m[1][1], obb_.rotate.m[1][2] };
			Vector3 axesZ = { obb_.rotate.m[2][0], obb_.rotate.m[2][1], obb_.rotate.m[2][2] };
			
			float hx = std::abs(axesX.x) * obb_.size.x + std::abs(axesY.x) * obb_.size.y + std::abs(axesZ.x) * obb_.size.z;
			float hy = std::abs(axesX.y) * obb_.size.x + std::abs(axesY.y) * obb_.size.y + std::abs(axesZ.y) * obb_.size.z;
			float hz = std::abs(axesX.z) * obb_.size.x + std::abs(axesY.z) * obb_.size.y + std::abs(axesZ.z) * obb_.size.z;
			
			Vector3 h = { hx, hy, hz };
			return ::AABB(obb_.center - h, obb_.center + h);
		}
		
		/**
		 * @brief OBBデータを設定
		 * @param obb 設定するOBBデータ
		 */
		void SetOBB(const ::OBB& obb) { obb_ = obb; }
		
		/**
		 * @brief OBBデータを取得
		 * @return 現在のOBBデータ
		 */
		const ::OBB& GetOBB() const { return obb_; }

	private:
		// OBBデータ（中心位置、サイズ、回転行列）
		::OBB obb_;
	};
}


