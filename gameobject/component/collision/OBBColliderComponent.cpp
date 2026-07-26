#include "OBBColliderComponent.h"

// app
#include "engine/gameobject/base/GameObject.h"
// system
#include "manager/graphics/LineManager.h"
// math
#include "math/VectorColorCodes.h"

// Factory
#include "engine/gameobject/component/base/ComponentFactory.h"

namespace KCE
{
REGISTER_COMPONENT(OBBColliderComponent)

namespace GameObjectComponent
{
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

		// 前フレームの位置を記録しておく（サブステップ判定をワールド空間で行うため、ワールド座標で記録）
		previousPosition_ = MathUtils::GetTranslateFromMatrix(owner_->GetWorldMatrix());

		Register("sizeOffset", &sizeOffset_);
		Register("useSubstep", &useSubstep_);
	}

	OBBColliderComponent::~OBBColliderComponent()
	{

	}

	void OBBColliderComponent::Update(GameObject* owner)
	{
		// 非アクティブ時は更新もデバッグ描画も行わない
		if (!isActive_) return;

		if (owner && autoUpdatePosition_)
		{
			const Matrix4x4& m = owner->GetWorldMatrix();

			// ワールド行列から位置、回転、スケールを取得してOBBを更新
			obb_.center = MathUtils::GetTranslateFromMatrix(m);
			obb_.rotate = MathUtils::GetMatrixRotate(m);
			obb_.size = MathUtils::GetScaleFromMatrix(m) + sizeOffset_;
		}
		
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
}
} // namespace KCE
