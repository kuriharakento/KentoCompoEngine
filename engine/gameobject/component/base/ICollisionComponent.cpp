#include "ICollisionComponent.h"
#include "engine/gameobject/component/collision/CollisionManager.h"

namespace GameObjectComponent
{
	ICollisionComponent::~ICollisionComponent()
	{
		owner_ = nullptr;
		// CollisionManagerから自動登録解除
		CollisionManager::GetInstance()->Unregister(this);
	}

	ICollisionComponent::ICollisionComponent(::GameObject* owner)
	{
		owner_ = owner;
		// CollisionManagerに自動登録
		CollisionManager::GetInstance()->Register(this);
	}
}

