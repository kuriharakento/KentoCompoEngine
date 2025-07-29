#pragma once
// camerawork
#include "camerawork/base/CameraWorkBase.h"

class FollowCamera : public CameraWorkBase {
public:
    FollowCamera() = default;
    void Initialize(Camera* camera) override;
    void Start(const Vector3* target, float distance, float sensitivity = 0.1f);
    void Update() override;
    void Stop();

private:
    const Vector3* target_ = nullptr; // 追従するターゲットの座標
    float distance_ = 10.0f;          // ターゲットとの距離
    float sensitivity_ = 0.1f;        // マウス感度
    float yaw_ = 0.0f;                // 水平方向の角度
    float pitch_ = 0.0f;              // 垂直方向の角度
    bool isActive_ = false;           // カメラがアクティブかどうか
    Camera* camera_ = nullptr;        // 操作対象のカメラ
};
