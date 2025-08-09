#include "AssaultEnemy.h"

#include "application/GameObject/component/action/AssaultEnemyBehavior.h"
#include "application/GameObject/component/action/AssaultRifleComponent.h"
#include "application/GameObject/component/action/GravityPhysicsComponent.h"
#include "application/GameObject/component/collision/OBBColliderComponent.h"

void AssaultEnemy::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target)
{
	EnemyBase::Initialize(object3dCommon, lightManager, target);
	// AssaultRifleのコンポーネントを追加
	AddComponent("AssaultRifleComponent", std::make_unique<AssaultRifleComponent>(object3dCommon, lightManager));
	// ビヘイビアコンポーネントを追加
	AddComponent("AssaultEnemyBehavior", std::make_unique<AssaultEnemyBehavior>(target_));
	// 重力演算コンポーネントを追加
	AddComponent("GravityPhysicsComponent", std::make_unique<GravityPhysicsComponent>());
	// OBBコライダーコンポーネントを追加
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
	// スイープ判定を使用
	collider->SetUseSubstep(true);
	// 衝突時の処理を設定
	collider->SetOnEnter([this](GameObject* other) {
		// 衝突した瞬間の処理
		if (other->GetTag() == "PlayerBullet")
		{
			hp_ -= 10.0f; // 仮のダメージ処理
		}
						 });
	collider->SetOnStay([this](GameObject* other) {
		// 衝突中の処理
						});
	collider->SetOnExit([this](GameObject* other) {
		// 衝突が離れた時の処理
						});
}
