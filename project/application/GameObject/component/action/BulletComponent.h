#pragma once
#include "application/GameObject/component/base/IActionComponent.h"
#include "math/Vector3.h"

class BulletComponent : public IActionComponent
{
public:
	BulletComponent();
	void Initialize(Vector3 direction, float speed, float lifetime);
	void Update(GameObject* owner) override;
	void Draw(CameraManager* camera) override {};

private:
	Vector3 direction_;  // 弾の進行方向
	float speed_;        // 弾の移動速度
	float lifetime_;     // 弾の寿命
	float timeAlive_;    // 経過時間
};
