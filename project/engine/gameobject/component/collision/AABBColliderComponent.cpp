#include "AABBColliderComponent.h"

// app
#include "engine/gameobject/base/GameObject.h"
// system
#include "manager/graphics/LineManager.h"
// math
#include "math/VectorColorCodes.h"

namespace GameObjectComponent
{
	AABBColliderComponent::AABBColliderComponent(::GameObject* owner) : ICollisionComponent(owner), aabb_(Vector3(), Vector3())
	{
		// GameObjectの位置とスケールからAABBを初期化
		aabb_.min_ = owner->GetPosition() - owner->GetScale();
		aabb_.max_ = owner->GetPosition() + owner->GetScale();
	}

	AABBColliderComponent::~AABBColliderComponent()
	{
		
	}

	void AABBColliderComponent::Update(::GameObject* owner)
	{
		Vector3 pos = owner->GetPosition();
		Vector3 size = owner->GetScale();
		
		// サイズオフセットを適用してAABBを更新
		aabb_.min_ = pos - (size + sizeOffset_);
		aabb_.max_ = pos + (size + sizeOffset_);
		
	#ifdef _DEBUG
		// デバッグモードでAABBを可視化
		LineManager::GetInstance()->DrawAABB(aabb_, VectorColorCodes::Cyan);
	#endif
	}
}

