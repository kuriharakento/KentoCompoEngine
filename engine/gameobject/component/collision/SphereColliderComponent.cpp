#include "SphereColliderComponent.h"
#include "engine/gameobject/base/GameObject.h"
#include "manager/graphics/LineManager.h"
#include "math/VectorColorCodes.h"

// Factory
#include "engine/gameobject/component/base/ComponentFactory.h"
REGISTER_COMPONENT(SphereColliderComponent)

namespace GameObjectComponent
{
	SphereColliderComponent::SphereColliderComponent(::GameObject* owner)
		: ICollisionComponent(owner), sphere_()
	{
		Register("radius", &sphere_.radius);
		Register("sizeOffset", &sizeOffset_);
		Register("useSubstep", &useSubstep_);
	}

	const ::Sphere& SphereColliderComponent::GetSphere() const
	{
		return sphere_;
	}

	void SphereColliderComponent::SetSphere(const ::Sphere& s)
	{
		sphere_ = s;
	}

	void SphereColliderComponent::Update(::GameObject* owner)
	{
		if (owner)
		{
			// GameObjectの位置に合わせて球の中心を更新
			sphere_.center = owner->GetPosition();
		}

	#ifdef _DEBUG
		// デバッグモードで球を可視化
		LineManager::GetInstance()->DrawSphere(
			sphere_.center, 
			sphere_.radius,
			VectorColorCodes::Yellow
		);
	#endif
	}

	ColliderType SphereColliderComponent::GetColliderType() const
	{
		return ColliderType::Sphere;
	}
}

