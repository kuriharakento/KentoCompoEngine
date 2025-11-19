#include "ShotgunEnemy.h"

#include "application/GameObject/component/action/ShotgunComponent.h"
#include "application/GameObject/component/collision/OBBColliderComponent.h"

void ShotgunEnemy::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target, const Transform& initialTransform)
{
	EnemyBase::Initialize(object3dCommon, lightManager, target, initialTransform);
	
	// コンポーネントの追加
	AddComponent("ShotgunComponent", std::make_unique<ShotgunComponent>(object3dCommon, lightManager));
	
	// 衝突判定の設定
	auto collider = std::make_unique<OBBColliderComponent>(this);
	collider->SetOnEnter([this](GameObject* other) {
		if (other->GetTag() == GameObjectTag::Weapon::PlayerBullet)
		{
			SetAlive(false);
		}
						});
	collider->SetOnStay([this](GameObject* other) {
						});
	collider->SetOnExit([this](GameObject* other) {
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