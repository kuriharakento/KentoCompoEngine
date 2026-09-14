#include "AABBCollider.h"

// app
#include "engine/gameobject/base/GameObject.h"
// system
#include "manager/graphics/LineManager.h"
// math
#include "math/VectorColorCodes.h"
#include "math/MathUtils.h"

// Factory
#include "engine/gameobject/component/base/ComponentFactory.h"

namespace KCE
{
REGISTER_COMPONENT(AABBCollider)
REGISTER_COMPONENT_ALIAS(AABBCollider, AABBColliderComponent)

namespace GameObjectComponent
{
	AABBCollider::AABBCollider() : aabb_(Vector3(), Vector3())
	{
		Register("sizeOffset", &sizeOffset_);
		Register("center", &centerOffset_);
		Register("useSubstep", &useSubstep_);
		// プレハブで作り直したときに、当たる相手の設定が既定値（None）に戻らないよう保存する
		Register("collisionLayer", &layer_);
		Register("collisionMask", &collisionMask_);
	}

	AABBCollider::~AABBCollider()
	{

	}

	void AABBCollider::Update()
	{
		// 非アクティブ時は更新もデバッグ描画も行わない


		if (GetOwner() && autoUpdatePosition_)
		{
			const Matrix4x4 world = GetOwner()->GetWorldMatrix();
			Vector3 pos = MathUtils::Transform(centerOffset_, world);
			Vector3 size = MathUtils::GetScaleFromMatrix(world);

			// サイズオフセットを適用してAABBを更新
			aabb_.min_ = pos - (size + sizeOffset_);
			aabb_.max_ = pos + (size + sizeOffset_);
		}

	#ifdef _DEBUG
		// デバッグモードでAABBを可視化
		LineManager::GetInstance()->DrawAABB(aabb_, KCE::VectorColorCodes::Cyan);
	#endif
	}
}
} // namespace KCE
