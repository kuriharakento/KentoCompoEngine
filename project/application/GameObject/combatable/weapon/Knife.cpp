#include "Knife.h"

void Knife::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager)
{
	GameObject::Initialize(object3dCommon, lightManager); // GameObjectの初期化
	isAlive_ = true; // ナイフは初期状態で生存
}

void Knife::Update()
{
	GameObject::Update();
}

void Knife::Draw(CameraManager* camera)
{
	if (!isAlive_) return; // ナイフが生存していない場合は描画しない
	GameObject::Draw(camera); // GameObjectの描画
}
