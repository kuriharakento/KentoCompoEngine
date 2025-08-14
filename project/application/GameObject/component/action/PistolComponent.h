#pragma once
#include <memory>

#include "application/GameObject/Combatable/weapon/Bullet.h"
#include "application/GameObject/component/base/IActionComponent.h"
#include "input/Input.h"
#include "math/MathUtils.h"
#include "math/Vector3.h"

class PistolComponent : public IActionComponent
{
public:
	PistolComponent(Object3dCommon* object3dCommon, LightManager* lightManager);
	~PistolComponent();

	void Update(GameObject* owner) override;
	void Draw(CameraManager* camera) override;

private:
	void FireBullet(GameObject* owner);
	void FireBullet(GameObject* owner, const Vector3& targetPosition);
	void StartReload();
	void Reload(float deltaTime);
	Object3dCommon* object3dCommon_ = nullptr;
	LightManager* lightManager_ = nullptr;

	float fireCooldown_;  // 発射のクールダウン時間
	float fireCooldownTimer_;  // 現在のクールダウンタイマー
	std::vector<std::unique_ptr<Bullet>> bullets_;  // 発射された弾のリスト

	int maxAmmo_ = 12;           // マガジン最大弾数（例: 12発）
	int currentAmmo_ = 12;       // 現在の弾数
	bool isReloading_ = false;   // リロード中か
	float reloadTime_ = 1.5f;    // リロードにかかる時間（例: 1.5秒）
	float reloadTimer_ = 0.0f;   // リロード経過時間
};
