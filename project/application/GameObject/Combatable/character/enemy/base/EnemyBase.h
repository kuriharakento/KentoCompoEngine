#pragma once
#include "application/gameObject/combatable/character/base/Character.h"
#include <graphics/2d/SpriteCommon.h>
#include "application/gameObject/base/GameObjectTag.h"

class EnemyBase : virtual public Character
{
public:
	EnemyBase() : Character(gameObjectTag::common::EnemyBase) {}
	virtual ~EnemyBase(); // デストラクタを実装（タイマー削除のため）
	virtual void Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, CameraManager* camera, LightManager* lightManager, GameObject* target, const Transform& initialTransform = Transform());
	void Update() override;
	void TakeDamage(float damage) override; // ダメージ処理のオーバーライド

	GameObject* GetTarget() const { return target_; }

protected:
	GameObject* target_ = nullptr; // ターゲットとなるプレイヤーや他のオブジェクト
};

