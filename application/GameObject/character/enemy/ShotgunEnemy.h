#pragma once
#include "base/EnemyBase.h"

class ShotgunEnemy : public EnemyBase
{
public:
	ShotgunEnemy() : Character("ShotgunEnemy") {}
	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target) override;
	void Update() override;
	void Draw(CameraManager* camera) override;
};

