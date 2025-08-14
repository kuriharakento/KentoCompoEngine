#pragma once
#include "application/GameObject/Combatable/character/base/Character.h"

class EnemyBase : virtual public Character
{
public:
	EnemyBase() : Character("EnemyBase") {}
	virtual ~EnemyBase() = default;
	virtual void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target);
	void Update() override;
	void Draw(CameraManager* camera) override;

	GameObject* GetTarget() const { return target_; }

protected:
	GameObject* target_ = nullptr; // ターゲットとなるプレイヤーや他のオブジェクト
};

