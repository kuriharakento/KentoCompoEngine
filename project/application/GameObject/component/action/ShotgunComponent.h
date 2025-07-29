#pragma once

#include "application/GameObject/component/base/IActionComponent.h"
#include "application/GameObject/weapon/Bullet.h"

class ShotgunComponent : public IActionComponent
{
public:
    ShotgunComponent(Object3dCommon* object3dCommon, LightManager* lightManager);
    ~ShotgunComponent();

    void Update(GameObject* owner) override;
    void Draw(CameraManager* camera) override;

private:
    void FireBullets(GameObject* owner);
    void FireBullets(GameObject* owner, const Vector3& targetPosition);
	void StartReload();
	void Reload(float deltaTime);

    Object3dCommon* object3dCommon_ = nullptr;
    LightManager* lightManager_ = nullptr;

    float fireCooldown_;
    float fireCooldownTimer_;
    std::vector<std::unique_ptr<Bullet>> bullets_;
    int pelletCount_ = 5; // 1発で発射する弾数
    float spreadAngle_ = 25.0f; // 扇状の角度（度数法）

    int maxAmmo_ = 6;              // マガジン最大弾数（例: 6発）
    int currentAmmo_ = 6;          // 現在の弾数
    bool isReloading_ = false;     // リロード中か
    float reloadTime_ = 2.5f;      // リロードにかかる時間（例: 2.5秒）
    float reloadTimer_ = 0.0f;     // リロード経過時間
};
