#include "AABBColliderComponent.h"

// app
#include "application/GameObject/base/GameObject.h"
// system
#include "manager/graphics/LineManager.h"
// math
#include "math/VectorColorCodes.h"

AABBColliderComponent::AABBColliderComponent(GameObject* owner) : ICollisionComponent(owner), aabb_(Vector3(), Vector3())
{
	// AABBの初期化
	aabb_.min_ = owner->GetPosition() - owner->GetScale();
	aabb_.max_ = owner->GetPosition() + owner->GetScale();
}

AABBColliderComponent::~AABBColliderComponent()
{
	
}

void AABBColliderComponent::Update(GameObject* owner)
{
	// オーナーの位置とスケールを取得
	Vector3 pos = owner->GetPosition();
	Vector3 size = owner->GetScale();
	// AABBの更新
	aabb_.min_ = pos - (size + sizeOffset_); // サイズオフセットを適用
	aabb_.max_ = pos + (size + sizeOffset_); // サイズオフセットを適用
#ifdef _DEBUG
	//AABBを可視化する
	LineManager::GetInstance()->DrawAABB(aabb_, VectorColorCodes::Cyan);
#endif
}
