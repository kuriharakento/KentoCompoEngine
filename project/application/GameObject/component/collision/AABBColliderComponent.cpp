#include "AABBColliderComponent.h"

// app
#include "application/GameObject/base/GameObject.h"
// system
#include "manager/graphics/LineManager.h"
// math
#include "math/VectorColorCodes.h"

/**
 * @brief コンストラクタ
 * 
 * GameObjectの位置とスケールからAABBを初期化します。
 */
AABBColliderComponent::AABBColliderComponent(GameObject* owner) : ICollisionComponent(owner), aabb_(Vector3(), Vector3())
{
	// GameObjectの位置とスケールからAABBを初期化
	aabb_.min_ = owner->GetPosition() - owner->GetScale();
	aabb_.max_ = owner->GetPosition() + owner->GetScale();
}

/**
 * @brief デストラクタ
 */
AABBColliderComponent::~AABBColliderComponent()
{
	
}

/**
 * @brief 毎フレームの更新処理
 * 
 * GameObjectの位置とスケールに合わせてAABBを更新します。
 */
void AABBColliderComponent::Update(GameObject* owner)
{
	// GameObjectの現在位置とスケールを取得
	Vector3 pos = owner->GetPosition();
	Vector3 size = owner->GetScale();
	
	// サイズオフセットを適用してAABBのmin/maxを計算
	aabb_.min_ = pos - (size + sizeOffset_);
	aabb_.max_ = pos + (size + sizeOffset_);
	
#ifdef _DEBUG
	// デバッグモードでAABBを可視化
	LineManager::GetInstance()->DrawAABB(aabb_, VectorColorCodes::Cyan);
#endif
}
