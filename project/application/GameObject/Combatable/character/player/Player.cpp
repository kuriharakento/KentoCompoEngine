#include "Player.h"

#include "application/GameObject/component/action/AssaultRifleComponent.h"
#include "application/GameObject/component/action/GravityPhysicsComponent.h"
#include "application/GameObject/component/action/PistolComponent.h"
#include "application/GameObject/component/action/MoveComponent.h"
#include "application/GameObject/component/base/ICollisionComponent.h"
#include "application/GameObject/component/collision/CollisionUtils.h"
#include "application/GameObject/component/collision/OBBColliderComponent.h"
#include "math/VectorColorCodes.h"

void Player::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, EnemyManager* enemyManager, CameraManager* camera)
{
	Character::Initialize(object3dCommon, lightManager);
	transform_.translate = { 0.0f, 1.0f, 0.0f };

	// 右腕オブジェクトの生成（武器の親オブジェクト）
	auto armR = std::make_unique<GameObject>(GameObjectTag::Character::PlayerRightArm);
	armR->Initialize(object3dCommon, lightManager);
	armR->SetModel("cube");
	armR->SetPosition(Vector3(3.0f, 0.0f, 0.0f));
	armR->AddComponent("OBBColliderComponent", std::make_unique<OBBColliderComponent>(armR.get()));
	armR->GetModel()->SetColor(VectorColorCodes::Red);
	AddChild(GameObjectTag::Character::PlayerRightArm, std::move(armR));

	// 左腕オブジェクトの生成
	auto armL = std::make_unique<GameObject>(GameObjectTag::Character::PlayerLeftArm);
	armL->Initialize(object3dCommon, lightManager);
	armL->SetModel("cube");
	armL->SetPosition(Vector3(-3.0f, 0.0f, 0.0f));
	armL->GetModel()->SetColor(VectorColorCodes::Blue);
	armL->AddComponent("OBBColliderComponent", std::make_unique<OBBColliderComponent>(armL.get()));
	AddChild(GameObjectTag::Character::PlayerLeftArm, std::move(armL));

	// コンポーネントの追加
	AddComponent("MoveComponent", std::make_unique<MoveComponent>(enemyManager, camera));
	AddComponent("GravityPhysicsComponent", std::make_unique<GravityPhysicsComponent>());
	AddComponent("PistolComponent", std::make_unique<AssaultRifleComponent>(object3dCommon, lightManager));
	AddComponent("OBBColliderComponent", std::make_unique<OBBColliderComponent>(this));
}

void Player::Update()
{
	Character::Update();
}

void Player::Draw(CameraManager* camera)
{
	Character::Draw(camera);
}

void Player::CollisionSettings(ICollisionComponent* collider)
{
	// スイープ判定を使用（高速移動時の衝突漏れ防止）
	collider->SetUseSubstep(true);

	// 衝突時の処理を設定
	collider->SetOnEnter([this](GameObject* other) {
		if (other->GetTag() == GameObjectTag::Weapon::EnemyBullet)
		{
			auto combatable = dynamic_cast<CombatableObject*>(other);
			if(combatable)
			{
				//TakeDamage(combatable->GetAttackPower());
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
