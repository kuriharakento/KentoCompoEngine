#include "Bullet.h"

void Bullet::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, const Vector3& position)
{
	GameObject::Initialize(object3dCommon, lightManager); // GameObjectの初期化
	SetPosition(position); // 弾の初期位置を設定
	isAlive_ = true;
}

void Bullet::Update(float deltaTime)
{
	GameObject::Update(); // GameObjectの更新
}

void Bullet::Draw(CameraManager* camera)
{
	// 弾の描画処理
	if (!isAlive_) return;
	GameObject::Draw(camera);
}
