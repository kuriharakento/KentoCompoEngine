#include "ICollisionComponent.h"
#include "application/GameObject/component/collision/CollisionManager.h"

ICollisionComponent::~ICollisionComponent()
{
	// 所有者への参照をクリア
	owner_ = nullptr;
	// CollisionManagerから自動登録解除（他のコライダーとの衝突判定を停止）
	CollisionManager::GetInstance()->Unregister(this);
}

ICollisionComponent::ICollisionComponent(GameObject* owner)
{
	// 所有者のGameObjectを保存
	owner_ = owner;
	// CollisionManagerに自動登録（毎フレームの衝突判定対象になる）
	CollisionManager::GetInstance()->Register(this);
}
