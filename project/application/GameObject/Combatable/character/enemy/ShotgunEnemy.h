#pragma once
#include "base/EnemyBase.h"

class ShotgunEnemy : public EnemyBase
{
public:
	ShotgunEnemy() : Character(gameObjectTag::character::ShotgunEnemy) {}
	void Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, CameraManager* camera, LightManager* lightManager, GameObject* target, const Transform& initialTransform = Transform()) override;
};

