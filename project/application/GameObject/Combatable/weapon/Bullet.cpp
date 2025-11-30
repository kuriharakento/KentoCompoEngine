#include "Bullet.h"

void Bullet::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, const Vector3& position)
{
	GameObject::Initialize(object3dCommon, lightManager);
	SetPosition(position);
}