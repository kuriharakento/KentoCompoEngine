#pragma once
#include "application/GameObject/character/enemy/base/EnemyBase.h"

class PistolEnemy : public EnemyBase
{
public:
	PistolEnemy() : Character("PistolEnemy") {}
	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target) override;
	void Update() override;
	void Draw(CameraManager* camera) override;
};

