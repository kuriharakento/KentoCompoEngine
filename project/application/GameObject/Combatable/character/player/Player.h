#pragma once
#include "application/GameObject/Combatable/character/base/Character.h"
#include "application/GameObject/component/base/ICollisionComponent.h"
#include "engine/graphics/2d/SpriteCommon.h"

class EnemyManager;
class WeaponManagerComponent;
class IWeaponComponent;

class Player : public Character
{
public:
	~Player() override;
	Player(std::string tag = GameObjectTag::Character::Player);
	void Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, LightManager* lightManager, EnemyManager* enemyManager, CameraManager* camera);
	void TakeDamage(float damage) override;

	/**
	 * @brief 武器管理コンポーネントを取得
	 * @return 武器管理コンポーネントへのポインタ
	 */
	WeaponManagerComponent* GetWeaponManager() const;

	/**
	 * @brief 現在装備中の武器を取得
	 * @return 現在の武器コンポーネント（なければnullptr）
	 */
	IWeaponComponent* GetCurrentWeapon() const;

private:
	void CollisionSettings(ICollisionComponent* collider) override;
};
