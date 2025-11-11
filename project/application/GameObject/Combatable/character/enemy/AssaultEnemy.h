#pragma once
#include "base/EnemyBase.h"

class AssaultEnemy : public EnemyBase
{
public:
	AssaultEnemy() : Character(GameObjectTag::Character::AssaultEnemy) {}
	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target, const Transform& initialTransform = Transform()) override;
	void Update() override;
	void Draw(CameraManager* camera) override;
	void CollisionSettings(ICollisionComponent* collider) override;
};

