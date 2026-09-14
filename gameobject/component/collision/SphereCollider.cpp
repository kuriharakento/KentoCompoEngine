#include "SphereCollider.h"
#include "engine/gameobject/base/GameObject.h"
#include "manager/graphics/LineManager.h"
#include "math/VectorColorCodes.h"
#include "math/MathUtils.h"

// Factory
#include "engine/gameobject/component/base/ComponentFactory.h"

namespace KCE
{
REGISTER_COMPONENT(SphereCollider)
REGISTER_COMPONENT_ALIAS(SphereCollider, SphereColliderComponent)

namespace GameObjectComponent
{
	SphereCollider::SphereCollider()
		: sphere_()
	{
		Register("radius", &sphere_.radius);
		Register("sizeOffset", &sizeOffset_);
		Register("center", &centerOffset_);
		Register("useSubstep", &useSubstep_);
		// プレハブで作り直したときに、当たる相手の設定が既定値（None）に戻らないよう保存する
		Register("collisionLayer", &layer_);
		Register("collisionMask", &collisionMask_);
	}

	const Sphere& SphereCollider::GetSphere() const
	{
		return sphere_;
	}

	void SphereCollider::SetSphere(const Sphere& s)
	{
		sphere_ = s;
	}

	void SphereCollider::Update()
	{
		// 非アクティブ時は更新もデバッグ描画も行わない


		// ownerが存在し、かつ自動更新が有効な場合のみ位置を同期する
		if (GetOwner() && autoUpdatePosition_)
		{
			// GameObjectの位置に合わせて球の中心を更新
			sphere_.center = MathUtils::Transform(centerOffset_, GetOwner()->GetWorldMatrix());
		}

	#ifdef _DEBUG
		// デバッグモードで球を可視化
		LineManager::GetInstance()->DrawSphere(
			sphere_.center,
			sphere_.radius,
			KCE::VectorColorCodes::Yellow
		);
	#endif
	}

	ColliderType SphereCollider::GetColliderType() const
	{
		return ColliderType::Sphere;
	}
}
} // namespace KCE
