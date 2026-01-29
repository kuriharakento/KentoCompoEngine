#pragma once

#include "base/EnemyBase.h"

class PistolEnemy : public EnemyBase
{
public:
	PistolEnemy() : Character(gameObjectTag::character::PistolEnemy) {}
	void Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, CameraManager* camera, LightManager* lightManager, GameObject* target, const Transform& initialTransform = Transform()) override;
};

