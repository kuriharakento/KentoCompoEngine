#include "OBBColliderComponent.h"

// app
#include "application/GameObject/base/GameObject.h"
// system
#include "manager/graphics/LineManager.h"
// math
#include "math/VectorColorCodes.h"

OBBColliderComponent::OBBColliderComponent(GameObject* owner) : ICollisionComponent(owner)
{
	// オーナーがセットされていない場合は何もしない
	if(!owner)
	{
		return;
	}

	// OBBの初期化
	obb_.center = owner->GetPosition();
	obb_.rotate = MakeRotateMatrix(owner->GetRotation());
	obb_.size = owner->GetScale();

	previousPosition_ = obb_.center; // 前の位置を初期化
}

OBBColliderComponent::~OBBColliderComponent()
{

}

void OBBColliderComponent::Update(GameObject* owner)
{
    // ワールド行列取得
    const Matrix4x4& m = owner->GetWorldMatrix();

    // OBBの更新
    obb_.center = MathUtils::GetTranslateFromMatrix(m);
    obb_.rotate = MathUtils::GetMatrixRotate(m);
    obb_.size = MathUtils::GetScaleFromMatrix(m) + sizeOffset_; // サイズオフセットを適用
	
#ifdef _DEBUG
	// OBBを可視化する
	LineManager::GetInstance()->DrawOBB(obb_, VectorColorCodes::Cyan);
	// 前の位置を可視化する
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
