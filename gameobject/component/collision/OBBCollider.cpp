#include "OBBCollider.h"

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
REGISTER_COMPONENT(OBBCollider)
REGISTER_COMPONENT_ALIAS(OBBCollider, OBBColliderComponent)

namespace GameObjectComponent
{
	OBBCollider::OBBCollider()
	{
		Register("sizeOffset", &sizeOffset_);
		Register("center", &centerOffset_);
		Register("useSubstep", &useSubstep_);
	}

	OBBCollider::~OBBCollider()
	{

	}

	void OBBCollider::Update()
	{
		// 非アクティブ時は更新もデバッグ描画も行わない


		if (GetOwner() && autoUpdatePosition_)
		{
			const Matrix4x4& m = GetOwner()->GetWorldMatrix();

			// ワールド行列から位置、回転、スケールを取得してOBBを更新
			obb_.center = MathUtils::Transform(centerOffset_, m);
			obb_.rotate = MathUtils::GetMatrixRotate(m);
			obb_.size = MathUtils::GetScaleFromMatrix(m) + sizeOffset_;
		}

	#ifdef _DEBUG
		// デバッグモードでOBBを可視化
		LineManager::GetInstance()->DrawOBB(obb_, KCE::VectorColorCodes::Cyan);

		// サブステップ判定使用時は前フレーム位置も可視化
		if (useSubstep_)
		{
			OBB previousObb = obb_;
			previousObb.center = previousPosition_;
			previousObb.rotate = obb_.rotate;
			previousObb.size = obb_.size;
			LineManager::GetInstance()->DrawOBB(previousObb, KCE::VectorColorCodes::Red);
		}
	#endif
	}
}
} // namespace KCE
