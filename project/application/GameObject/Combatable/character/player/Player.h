#pragma once
#include "application/GameObject/Combatable/character/base/Character.h"
#include "application/GameObject/component/base/ICollisionComponent.h"
#include "engine/graphics/2d/SpriteCommon.h"

class EnemyManager;

class Player : public Character
{
public:
	~Player() = default;
	Player(std::string tag = GameObjectTag::Character::Player) : Character(tag) {}
	void Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, LightManager* lightManager, EnemyManager* enemyManager, CameraManager* camera);

private:
	void CollisionSettings(ICollisionComponent* collider) override;
};
