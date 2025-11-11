#pragma once
#include "base/EnemyBase.h"

class KnifeEnemy : public EnemyBase
{
public:
	KnifeEnemy() : Character(GameObjectTag::Character::KnifeEnemy) {}
	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target, const Transform& initialTransform = Transform()) override;
	void Update() override;
	void Draw(CameraManager* camera) override;
	void CollisionSettings(ICollisionComponent* collider) override;
};

