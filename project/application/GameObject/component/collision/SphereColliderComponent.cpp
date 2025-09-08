#include "SphereColliderComponent.h"
#include "application/GameObject/base/GameObject.h"
#include "manager/graphics/LineManager.h"
#include "math/VectorColorCodes.h"

SphereColliderComponent::SphereColliderComponent(GameObject* owner)
    : ICollisionComponent(owner), sphere_()
{
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
	if (owner)
	{
		sphere_.center = owner->GetPosition();
	}

	// デバッグ描画
#ifdef _DEBUG
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
