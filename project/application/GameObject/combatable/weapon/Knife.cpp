#include "Knife.h"

void Knife::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager)
{
	GameObject::Initialize(object3dCommon, lightManager); // GameObjectの初期化
}

void Knife::Update()
{
	GameObject::Update();
}

void Knife::Draw(CameraManager* camera)
{
	if (!IsAlive()) return; // 生存していない場合は描画しない
	GameObject::Draw(camera); // GameObjectの描画
}
