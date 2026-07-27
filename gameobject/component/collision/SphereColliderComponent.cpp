#include "SphereColliderComponent.h"
#include "engine/gameobject/base/GameObject.h"
#include "manager/graphics/LineManager.h"
#include "math/VectorColorCodes.h"

// Factory
#include "engine/gameobject/component/base/ComponentFactory.h"

namespace KCE
{
REGISTER_COMPONENT(SphereColliderComponent)

namespace GameObjectComponent
{
	SphereColliderComponent::SphereColliderComponent(GameObject* owner)
		: ICollisionComponent(owner), sphere_()
	{
		Register("radius", &sphere_.radius);
		Register("sizeOffset", &sizeOffset_);
		Register("useSubstep", &useSubstep_);
	}

	const Sphere& SphereColliderComponent::GetSphere() const
	{
		return sphere_;
	}

	void SphereColliderComponent::SetSphere(const Sphere& s)
	{
		sphere_ = s;
	}

	void SphereColliderComponent::Update(GameObject* owner)
	{
		// 非アクティブ時は更新もデバッグ描画も行わない
		if (!isActive_) return;

		// ownerが存在し、かつ自動更新が有効な場合のみ位置を同期する
		if (owner && autoUpdatePosition_)
		{
			// GameObjectの位置に合わせて球の中心を更新
			sphere_.center = owner->GetPosition();
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

	ColliderType SphereColliderComponent::GetColliderType() const
	{
		return ColliderType::Sphere;
	}
}
} // namespace KCE
