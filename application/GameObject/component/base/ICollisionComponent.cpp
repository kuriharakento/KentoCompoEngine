#include "ICollisionComponent.h"
#include "application/GameObject/component/collision/CollisionManager.h"

ICollisionComponent::~ICollisionComponent()
{
	owner_ = nullptr;
	CollisionManager::GetInstance()->Unregister(this);
}

ICollisionComponent::ICollisionComponent(GameObject* owner)
{
	owner_ = owner;
	CollisionManager::GetInstance()->Register(this);
}
