#include "Bullet.h"

void Bullet::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, const Vector3& position)
{
	GameObject::Initialize(object3dCommon, lightManager);
	SetPosition(position);
}

Transform* Bullet::GetTransform() const
{
	// GameObject の transform_ は protected なので直接アクセス可能
	return const_cast<Transform*>(&transform_);
}

