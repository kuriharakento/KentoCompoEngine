#pragma once
#include "engine/gameobject/component/base/ICollisionComponent.h"
#include "math/Sphere.h"
#include "jsonEditor/JsonEditableBase.h"

/**
 * @brief 球（Sphere）による衝突判定コンポーネント
 * 
 * 球形状での衝突判定を行います。
 * 全方向に均等な判定を行うため、回転の影響を受けません。
 * 
 * 判定アルゴリズム:
 * - 中心間距離と半径の和を比較
 * - 最も高速な衝突判定方式
 * 
 * 用途:
 * - 球形のオブジェクト（弾丸、惑星、ボールなど）
 * - 大まかな衝突判定（広域フェーズ）
 * - 高速な判定が必要な場合
 * 
 * @note 2D/3Dモードでは円（Circle）として扱われます
 */
namespace GameObjectComponent
{
	class SphereColliderComponent : public ICollisionComponent, public JsonEditableBase
	{
	public:
		/**
		 * @brief コンストラクタ
		 * @param owner このコンポーネントを所有するGameObject
		 */
		SphereColliderComponent(::GameObject* owner);

		/**
		 * @brief 球データを取得
		 * @return 現在の球データ
		 */
		const ::Sphere& GetSphere() const;
		
		/**
		 * @brief 球データを設定
		 * @param s 設定する球データ
		 */
		void SetSphere(const ::Sphere& s);

		/**
		 * @brief 毎フレームの更新処理
		 * 
		 * GameObjectの位置に合わせて球の中心位置を更新します。
		 * 
		 * @param owner このコンポーネントを所有するGameObject
		 */
		void Update(::GameObject* owner) override;

		/**
		 * @brief コライダーの種類を取得
		 * @return ColliderType::Sphere
		 */
		ColliderType GetColliderType() const override;

	private:
		// 球データ（中心位置と半径）
		::Sphere sphere_;
	};
}