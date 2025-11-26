#include "SphereColliderComponent.h"
#include "application/GameObject/base/GameObject.h"
#include "manager/graphics/LineManager.h"
#include "math/VectorColorCodes.h"

/**
 * @brief コンストラクタ
 */
SphereColliderComponent::SphereColliderComponent(GameObject* owner)
    : ICollisionComponent(owner), sphere_()
{
}

/**
 * @brief 球データを取得
 */
const Sphere& SphereColliderComponent::GetSphere() const
{
    return sphere_;
}

/**
 * @brief 球データを設定
 */
void SphereColliderComponent::SetSphere(const Sphere& s)
{
    sphere_ = s;
}

/**
 * @brief 毎フレームの更新処理
 */
void SphereColliderComponent::Update(GameObject* owner)
{
	if (owner)
	{
		// GameObjectの位置に合わせて球の中心を更新
		sphere_.center = owner->GetPosition();
	}

#ifdef _DEBUG
	// デバッグモードで球を可視化
	LineManager::GetInstance()->DrawSphere(
		sphere_.center, 
		sphere_.radius,
		VectorColorCodes::Yellow
	);
#endif
}

/**
 * @brief コライダーの種類を取得
 */
ColliderType SphereColliderComponent::GetColliderType() const
{
    return ColliderType::Sphere;
}
