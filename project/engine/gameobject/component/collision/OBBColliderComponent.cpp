#include "OBBColliderComponent.h"

// app
#include "engine/gameobject/base/GameObject.h"
// system
#include "manager/graphics/LineManager.h"
// math
#include "math/VectorColorCodes.h"

OBBColliderComponent::OBBColliderComponent(GameObject* owner) : ICollisionComponent(owner)
{
	if(!owner)
	{
		return;
	}

	// GameObjectの位置、回転、スケールからOBBを初期化
	obb_.center = owner->GetPosition();
	obb_.rotate = MakeRotateMatrix(owner->GetRotation());
	obb_.size = owner->GetScale();

	previousPosition_ = obb_.center;
}

OBBColliderComponent::~OBBColliderComponent()
{

}

void OBBColliderComponent::Update(GameObject* owner)
{
	const Matrix4x4& m = owner->GetWorldMatrix();

	// ワールド行列から位置、回転、スケールを取得してOBBを更新
	obb_.center = MathUtils::GetTranslateFromMatrix(m);
	obb_.rotate = MathUtils::GetMatrixRotate(m);
	obb_.size = MathUtils::GetScaleFromMatrix(m) + sizeOffset_;
	
#ifdef _DEBUG
	// デバッグモードでOBBを可視化
	LineManager::GetInstance()->DrawOBB(obb_, VectorColorCodes::Cyan);
	
	// サブステップ判定使用時は前フレーム位置も可視化
	if (useSubstep_)
	{
		OBB previousObb = obb_;
		previousObb.center = previousPosition_;
		previousObb.rotate = obb_.rotate;
		previousObb.size = obb_.size;
		LineManager::GetInstance()->DrawOBB(previousObb, VectorColorCodes::Red);
	}
#endif
}
