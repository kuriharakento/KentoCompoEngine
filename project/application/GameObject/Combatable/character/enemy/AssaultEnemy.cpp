#include "AssaultEnemy.h"

#include "application/GameObject/component/action/AssaultEnemyBehavior.h"
#include "application/GameObject/component/action/AssaultRifleComponent.h"
#include "application/GameObject/component/action/GravityPhysicsComponent.h"
#include "application/GameObject/component/collision/OBBColliderComponent.h"

void AssaultEnemy::Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, CameraManager* camera, LightManager* lightManager, GameObject* target, const Transform& initialTransform)
{
	EnemyBase::Initialize(object3dCommon, spriteCommon, camera, lightManager, target, initialTransform);

	// スキニングモデルを設定（フォルダ名のみ指定）
	SetSkinnedModel("walk");

	// AssaultRifleのコンポーネントを追加
	AddComponent("AssaultRifleComponent", std::make_unique<AssaultRifleComponent>(object3dCommon, lightManager));
	// ビヘイビアコンポーネントを追加
	AddComponent("AssaultEnemyBehavior", std::make_unique<AssaultEnemyBehavior>(target_));
	// 重力演算コンポーネントを追加
	AddComponent("GravityPhysicsComponent", std::make_unique<GravityPhysicsComponent>());
	// OBBコライダーコンポーネントを追加
	AddComponent("OBBColliderComponent", std::make_unique<OBBColliderComponent>(this));
}

void AssaultEnemy::CollisionSettings(ICollisionComponent* collider)
{
	// スイープ判定を使用
	collider->SetUseSubstep(true);
	// 衝突時の処理を設定
	collider->SetOnEnter([this](GameObject* other) {
		// 衝突した瞬間の処理
		if (other->GetTag() == gameObjectTag::weapon::PlayerBullet)
		{
			auto combatable = dynamic_cast<CombatableObject*>(other);
			if (combatable)
			{
				TakeDamage(combatable->GetAttackPower());
			}
		}
						 });
	collider->SetOnStay([this](GameObject* other) {
		// 衝突中の処理
						});
	collider->SetOnExit([this](GameObject* other) {
		// 衝突が離れた時の処理
						});
}
