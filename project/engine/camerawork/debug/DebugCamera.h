#pragma once
// camerawork
#include "camerawork/base/CameraWorkBase.h"
// math
#include "math/Vector3.h"

class DebugCamera : public CameraWorkBase
{
public:
    DebugCamera() = default;
    void Initialize(Camera* camera) override;
    void Update() override;
    void Start(const Vector3& initialPosition = { 0.0f, 50.0f, -10.0f },
               const Vector3& initialRotation = { 1.1f, 0.0f, 0.0f });
    void Stop();
    void Reset(); // 初期位置・向きに戻る
    void FocusOnTarget(const Vector3& target); // ターゲットにフォーカス

    // 設定用アクセサ
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }
    void SetMouseSensitivity(float sensitivity) { mouseSensitivity_ = sensitivity; }
    void SetSpeedMultiplier(float multiplier) { speedMultiplier_ = multiplier; }

    // 状態取得
    bool IsActive() const { return isActive_; }
    float GetMoveSpeed() const { return moveSpeed_; }
    float GetMouseSensitivity() const { return mouseSensitivity_; }

private:
    void UpdateMovement();
    void UpdateMouseLook();
    void UpdateSpeedControl();
    void DrawDebugUI();

    Vector3 GetForwardVector() const;
    Vector3 GetRightVector() const;
    Vector3 GetUpVector() const;

private:
    // 移動パラメータ
    float moveSpeed_ = 5.0f;        // 基本移動速度
    float fastMoveMultiplier_ = 3.0f;  // 高速移動時の倍率
    float slowMoveMultiplier_ = 0.3f;  // 低速移動時の倍率
    float speedMultiplier_ = 3.0f;     // 現在の速度倍率

    // マウス設定
    float mouseSensitivity_ = 0.05f;

    // 回転状態
    float yaw_ = 0.0f;     // 水平回転
    float pitch_ = 0.0f;   // 垂直回転

    // 状態フラグ
    bool isActive_ = false;
    bool showDebugUI_ = true;

	// キー状態（前回フレームとの比較用）
    bool prevTabPressed_ = false;
};