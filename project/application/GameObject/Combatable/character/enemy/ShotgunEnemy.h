#pragma once
#include "base/EnemyBase.h"

class ShotgunEnemy : public EnemyBase
{
public:
	ShotgunEnemy() : Character(GameObjectTag::Character::ShotgunEnemy) {}
	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target, const Transform& initialTransform = Transform()) override;
};

