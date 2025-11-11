#include "EnemyBase.h"

#include "time/TimeManager.h"

void EnemyBase::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target, const Transform& initialTransform)
{
	Character::Initialize(object3dCommon, lightManager, initialTransform);

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
