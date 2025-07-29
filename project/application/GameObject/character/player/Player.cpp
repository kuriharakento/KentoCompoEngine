#include "Player.h"

#include "application/GameObject/component/action/AssaultRifleComponent.h"
#include "application/GameObject/component/action/GravityPhysicsComponent.h"
#include "application/GameObject/component/action/PistolComponent.h"
#include "application/GameObject/component/action/MoveComponent.h"
#include "application/GameObject/component/base/ICollisionComponent.h"
#include "application/GameObject/component/collision/CollisionUtils.h"
#include "application/GameObject/component/collision/OBBColliderComponent.h"
#include "base/Logger.h"

void Player::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager)
{
	Character::Initialize(object3dCommon, lightManager);
	//初期位置を設定
	transform_.translate = { 0.0f, 1.0f, 0.0f };

	// 移動コンポーネントを追加
	AddComponent("MoveComponent", std::make_unique<MoveComponent>());
	// 重力演算コンポーネントを追加
	AddComponent("GravityPhysicsComponent", std::make_unique<GravityPhysicsComponent>());
	// 射撃コンポーネントを追加
	AddComponent("PistolComponent", std::make_unique<AssaultRifleComponent>(object3dCommon, lightManager));
	// 衝突判定コンポーネント
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
	// スイープ判定を仕様
	collider->SetUseSweep(true);

	// 衝突時の処理を設定
	collider->SetOnEnter([this](GameObject* other) {
		// 衝突した瞬間の処理
		
						 });
	collider->SetOnStay([this](GameObject* other) {
		// 衝突中の処理
		
						});
	collider->SetOnExit([this](GameObject* other) {
		// 衝突が離れた時の処理

						});
}
