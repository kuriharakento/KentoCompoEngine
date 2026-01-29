#include "Player.h"

#include "application/GameObject/component/action/AssaultRifleComponent.h"
#include "application/GameObject/component/action/GravityPhysicsComponent.h"
#include "application/GameObject/component/action/PistolComponent.h"
#include "application/GameObject/component/action/MoveComponent.h"
#include "application/GameObject/component/action/ShotgunComponent.h"
#include "application/GameObject/component/action/WeaponManagerComponent.h"
#include "application/GameObject/component/base/ICollisionComponent.h"
#include "application/GameObject/component/collision/CollisionUtils.h"
#include "application/GameObject/component/collision/OBBColliderComponent.h"
#include "math/VectorColorCodes.h"
#include "application/GameObject/component/action/PlayerUIComponent.h"
#include "time/TimerManager.h"
#include <sstream>

// 定数定義
const float kHitFlashDuration = 0.05f; // 被弾時の赤色表示時間

// タイマー名を生成するヘルパー
static std::string GenerateHitTimerName(const Player* player)
{
	std::stringstream ss;
	ss << "Player_HitFlash_" << player;
	return ss.str();
}

Player::Player(std::string tag) : Character(tag)
{
}

Player::~Player()
{
	TimerManager::GetInstance().RemoveTimer(GenerateHitTimerName(this));
}

void Player::TakeDamage(float damage)
{
	CombatableObject::TakeDamage(damage);

	// ヒット時のフラッシュ処理（TimerManagerを使用）
	std::string timerName = GenerateHitTimerName(this);
	Timer* timer = TimerManager::GetInstance().GetTimer(timerName);

	if (timer)
	{
		// 既にタイマーがある場合はリセットして延長
		timer->Reset();
		timer->Start();
	}
	else
	{
		// 新しいタイマーを作成
		auto newTimer = std::make_unique<Timer>(timerName, kHitFlashDuration);

		// 色変更ヘルパーラムダ
		auto setColorFunc = [this](const Vector4& color) {
			SetColor(color);
			// 子オブジェクトの色も変更
			for (auto& [name, child] : GetChildren()) {
				if (child) {
					child->SetColor(color);
				}
			}
		};

		// 開始時に赤くする
		newTimer->SetOnStart([setColorFunc]() {
			setColorFunc(VectorColorCodes::Red);
		});

		// 終了時に白に戻す
		newTimer->SetOnFinish([setColorFunc]() {
			setColorFunc(VectorColorCodes::White);
		});

		TimerManager::GetInstance().AddTimer(std::move(newTimer));
	}
}

WeaponManagerComponent* Player::GetWeaponManager() const
{
	auto component = GetComponent<WeaponManagerComponent>();
	return component.get();
}

IWeaponComponent* Player::GetCurrentWeapon() const
{
	auto* weaponManager = GetWeaponManager();
	if (weaponManager)
	{
		return weaponManager->GetCurrentWeapon();
	}
	return nullptr;
}

void Player::Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, LightManager* lightManager, EnemyManager* enemyManager, CameraManager* camera)
{
	Character::Initialize(object3dCommon, lightManager);

	// スキニングモデルを設定
	SetSkinnedModel("player_walk");

	// 初期位置を設定
	transform_.translate = { 0.0f, 1.0f, 0.0f };
	
	// 移動コンポーネントを追加
	AddComponent("MoveComponent", std::make_unique<MoveComponent>(enemyManager, camera));
	// アニメーションインデックスを設定（0番が歩きと仮定）
	if (auto moveComp = GetComponent<MoveComponent>())
	{
		moveComp->SetWalkAnimationIndex(0);
	}
	// 重力演算コンポーネントを追加
	AddComponent("GravityPhysicsComponent", std::make_unique<GravityPhysicsComponent>());

	// 武器管理コンポーネントを作成
	auto weaponManager = std::make_unique<WeaponManagerComponent>(object3dCommon, lightManager);
	// 武器を追加（WeaponManagerComponentが所有）
	weaponManager->AddWeapon(std::make_unique<AssaultRifleComponent>(object3dCommon, lightManager));
	weaponManager->AddWeapon(std::make_unique<ShotgunComponent>(object3dCommon, lightManager));
	AddComponent("WeaponManager", std::move(weaponManager));

	// 衝突判定コンポーネント
	AddComponent("OBBColliderComponent", std::make_unique<OBBColliderComponent>(this));
	// UIコンポーネント
	AddComponent("PlayerUIComponent", std::make_unique<PlayerUIComponent>(spriteCommon));
}

void Player::CollisionSettings(ICollisionComponent* collider)
{
	// スイープ判定を仕様
	collider->SetUseSubstep(true);

	// 衝突時の処理を設定
	collider->SetOnEnter([this](GameObject* other) {
		// 衝突した瞬間の処理
		if (other->GetTag() == gameObjectTag::weapon::EnemyBullet)
		{
			auto combatable = dynamic_cast<CombatableObject*>(other);
			if(combatable)
			{
				TakeDamage(combatable->GetAttackPower() / 10.0f);
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
