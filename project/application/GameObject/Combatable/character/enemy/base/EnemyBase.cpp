#include "EnemyBase.h"

#include "time/TimeManager.h"
#include <application/GameObject/component/action/EnemyUIComponent.h>

void EnemyBase::Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, CameraManager* camera, LightManager* lightManager, GameObject* target, const Transform& initialTransform)
{
	Character::Initialize(object3dCommon, lightManager, initialTransform);

	// UIコンポーネント
	auto uiComp = std::make_unique<EnemyUIComponent>(spriteCommon, camera);
	uiComp->SetScreenOffset({ -50.0f, 0.0f }); // スクリーンオフセットを設定
	AddComponent("EnemyUIComponent", std::move(uiComp));

	target_ = target; // ターゲットを設定
}

void EnemyBase::Update()
{
	Character::Update();
}