#include "AABBColliderComponent.h"

// app
#include "engine/gameobject/base/GameObject.h"
// system
#include "manager/graphics/LineManager.h"
// math
#include "math/VectorColorCodes.h"

// Factory
#include "engine/gameobject/component/base/ComponentFactory.h"
REGISTER_COMPONENT(AABBColliderComponent)

namespace GameObjectComponent
{
	AABBColliderComponent::AABBColliderComponent(::GameObject* owner) : ICollisionComponent(owner), aabb_(Vector3(), Vector3())
	{
		// GameObjectの位置とスケールからAABBを初期化
		aabb_.min_ = owner->GetPosition() - owner->GetScale();
		aabb_.max_ = owner->GetPosition() + owner->GetScale();

		Register("sizeOffset", &sizeOffset_);
		Register("useSubstep", &useSubstep_);
	}

	AABBColliderComponent::~AABBColliderComponent()
	{
		
	}

	void AABBColliderComponent::Update(::GameObject* owner)
	{
		// 非アクティブ時は更新もデバッグ描画も行わない
		if (!isActive_) return;

		if (owner && autoUpdatePosition_)
		{
			Vector3 pos = owner->GetPosition();
			Vector3 size = owner->GetScale();
			
			// サイズオフセットを適用してAABBを更新
			aabb_.min_ = pos - (size + sizeOffset_);
			aabb_.max_ = pos + (size + sizeOffset_);
		}
		
	#ifdef _DEBUG
		// デバッグモードでAABBを可視化
		LineManager::GetInstance()->DrawAABB(aabb_, VectorColorCodes::Cyan);
	#endif
	}
}

