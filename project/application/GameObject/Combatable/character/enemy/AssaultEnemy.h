#pragma once
#include "base/EnemyBase.h"

class AssaultEnemy : public EnemyBase
{
public:
	AssaultEnemy() : Character("AssaultEnemy") {}
	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target) override;
	void Update() override;
	void Draw(CameraManager* camera) override;
	void CollisionSettings(ICollisionComponent* collider) override;
};

