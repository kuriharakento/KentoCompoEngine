#pragma once
#include "application/GameObject/Combatable/character/base/Character.h"

class EnemyBase : virtual public Character
{
public:
	EnemyBase() : Character(GameObjectTag::Common::EnemyBase) {}
	virtual ~EnemyBase() = default;
	virtual void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target, const Transform& initialTransform = Transform());
	void Update() override;
	GameObject* GetTarget() const { return target_; }

protected:
	GameObject* target_ = nullptr; // ターゲットとなるプレイヤーや他のオブジェクト
};

