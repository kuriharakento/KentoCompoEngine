#include "AssaultEnemy.h"

#include "application/GameObject/component/action/AssaultEnemyBehavior.h"
#include "application/GameObject/component/action/AssaultRifleComponent.h"
#include "application/GameObject/component/action/GravityPhysicsComponent.h"
#include "application/GameObject/component/collision/OBBColliderComponent.h"

void AssaultEnemy::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target, const Transform& initialTransform)
{
	EnemyBase::Initialize(object3dCommon, lightManager, target, initialTransform);
	
	// コンポーネントの追加
	AddComponent("AssaultRifleComponent", std::make_unique<AssaultRifleComponent>(object3dCommon, lightManager));
	AddComponent("AssaultEnemyBehavior", std::make_unique<AssaultEnemyBehavior>(target_));
	AddComponent("GravityPhysicsComponent", std::make_unique<GravityPhysicsComponent>());
	AddComponent("OBBColliderComponent", std::make_unique<OBBColliderComponent>(this));
}

void AssaultEnemy::Update()
{
	EnemyBase::Update();
}

void AssaultEnemy::Draw(CameraManager* camera)
{
	EnemyBase::Draw(camera);
}

void AssaultEnemy::CollisionSettings(ICollisionComponent* collider)
{
	// スイープ判定を使用（高速移動時の衝突漏れ防止）
	collider->SetUseSubstep(true);
	
	collider->SetOnEnter([this](GameObject* other) {
		if (other->GetTag() == GameObjectTag::Weapon::PlayerBullet)
		{
			auto combatable = dynamic_cast<CombatableObject*>(other);
			if (combatable)
			{
				TakeDamage(combatable->GetAttackPower());
			}
		}
						 });
	collider->SetOnStay([this](GameObject* other) {
						});
	collider->SetOnExit([this](GameObject* other) {
						});
}
