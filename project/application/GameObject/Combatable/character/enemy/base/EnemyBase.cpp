#include "EnemyBase.h"

void EnemyBase::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target)
{
	Character::Initialize(object3dCommon, lightManager);

	target_ = target; // ターゲットを設定
}

void EnemyBase::Update()
{
	Character::Update();

	//　死亡処理(仮)
	if (hp_.base <= 0.0f)
	{
		isAlive_ = false; // 体力が0以下なら死亡
	}
}

void EnemyBase::Draw(CameraManager* camera)
{
	Character::Draw(camera);
}
