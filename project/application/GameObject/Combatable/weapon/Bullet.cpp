#include "Bullet.h"

void Bullet::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, const Vector3& position)
{
	GameObject::Initialize(object3dCommon, lightManager);
	SetPosition(position);
}

void Bullet::Update()
{
	GameObject::Update();
}

void Bullet::Draw(CameraManager* camera)
{
	if (!IsAlive()) return;
	GameObject::Draw(camera);
}
