#pragma once
#include "base/EnemyBase.h"

class AssaultEnemy : public EnemyBase
{
public:
	AssaultEnemy() : Character(GameObjectTag::Character::AssaultEnemy) {}
	void Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, CameraManager* camera, LightManager* lightManager, GameObject* target, const Transform& initialTransform = Transform()) override;
	void CollisionSettings(ICollisionComponent* collider) override;
};

