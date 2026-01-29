#include "ICollisionComponent.h"
#include "application/gameObject/component/collision/CollisionManager.h"

ICollisionComponent::~ICollisionComponent()
{
	owner_ = nullptr;
	// CollisionManagerから自動登録解除
	CollisionManager::GetInstance()->Unregister(this);
}

ICollisionComponent::ICollisionComponent(GameObject* owner)
{
	owner_ = owner;
	// CollisionManagerに自動登録
	CollisionManager::GetInstance()->Register(this);
}
