#include "OBBColliderComponent.h"

// app
#include "application/GameObject/base/GameObject.h"
// system
#include "manager/graphics/LineManager.h"
// math
#include "math/VectorColorCodes.h"

/**
 * @brief コンストラクタ
 * 
 * GameObjectの位置、回転、スケールからOBBを初期化します。
 */
OBBColliderComponent::OBBColliderComponent(GameObject* owner) : ICollisionComponent(owner)
{
	// nullptrチェック（テンポラリコライダー用）
	if(!owner)
	{
		return;
	}

	// GameObjectの位置、回転、スケールからOBBを初期化
	obb_.center = owner->GetPosition();
	obb_.rotate = MakeRotateMatrix(owner->GetRotation());
	obb_.size = owner->GetScale();

	// サブステップ判定用に前フレーム位置を初期化
	previousPosition_ = obb_.center;
}

/**
 * @brief デストラクタ
 */
OBBColliderComponent::~OBBColliderComponent()
{

}

/**
 * @brief 毎フレームの更新処理
 * 
 * GameObjectのワールド行列から位置、回転、スケールを取得してOBBを更新します。
 */
void OBBColliderComponent::Update(GameObject* owner)
{
	// ワールド行列を取得
	const Matrix4x4& m = owner->GetWorldMatrix();

	// ワールド行列から位置、回転、スケールを分解してOBBを更新
	obb_.center = MathUtils::GetTranslateFromMatrix(m);
	obb_.rotate = MathUtils::GetMatrixRotate(m);
	obb_.size = MathUtils::GetScaleFromMatrix(m) + sizeOffset_;
	
#ifdef _DEBUG
	// デバッグモードでOBBを可視化（シアン色）
	LineManager::GetInstance()->DrawOBB(obb_, VectorColorCodes::Cyan);
	
	// サブステップ判定使用時は前フレーム位置も可視化（赤色）
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
