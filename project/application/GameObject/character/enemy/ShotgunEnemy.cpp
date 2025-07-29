#include "ShotgunEnemy.h"

#include "application/GameObject/component/action/ShotgunComponent.h"
#include "application/GameObject/component/collision/OBBColliderComponent.h"

void ShotgunEnemy::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target)
{
	EnemyBase::Initialize(object3dCommon, lightManager, target);
	// Shotgunのコンポーネントを追加
	AddComponent("ShotgunComponent", std::make_unique<ShotgunComponent>(object3dCommon, lightManager));
	// 衝突判定コンポーネントを追加
	auto collider = std::make_unique<OBBColliderComponent>(this);
	collider->SetOnEnter([this](GameObject* other) {
		if (other->GetTag() == "PlayerBullet")
		{
			isAlive_ = false; // プレイヤーの弾に当たったら死亡
		}
						});
	collider->SetOnStay([this](GameObject* other) {
		// 衝突中の処理をここに記述
						});
	collider->SetOnExit([this](GameObject* other) {
		// 衝突終了時の処理をここに記述
						});
	AddComponent("OBBCollider", std::move(collider));
}

void ShotgunEnemy::Update()
{
	EnemyBase::Update();
}

void ShotgunEnemy::Draw(CameraManager* camera)
{
	EnemyBase::Draw(camera);
}