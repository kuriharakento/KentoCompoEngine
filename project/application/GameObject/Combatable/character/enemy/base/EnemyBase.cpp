#include "EnemyBase.h"

#include "time/TimeManager.h"

void EnemyBase::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target)
{
	Character::Initialize(object3dCommon, lightManager);

	target_ = target; // ターゲットを設定
}

void EnemyBase::Update()
{
	Character::Update();
}

void EnemyBase::Draw(CameraManager* camera)
{
	Character::Draw(camera);
}
