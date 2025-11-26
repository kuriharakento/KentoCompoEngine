#include "ICollisionComponent.h"
#include "application/GameObject/component/collision/CollisionManager.h"

/**
 * @brief デストラクタ
 * 
 * CollisionManagerから自動的に登録解除します。
 */
ICollisionComponent::~ICollisionComponent()
{
	owner_ = nullptr;
	// CollisionManagerから自動登録解除
	CollisionManager::GetInstance()->Unregister(this);
}

/**
 * @brief コンストラクタ
 * 
 * CollisionManagerに自動的に登録します。
 */
ICollisionComponent::ICollisionComponent(GameObject* owner)
{
	owner_ = owner;
	// CollisionManagerに自動登録
	CollisionManager::GetInstance()->Register(this);
}
