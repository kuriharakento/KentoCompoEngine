#pragma once
// camerawork
#include "camerawork/base/CameraWorkBase.h"

// math
#include "math/Vector3.h"
#include <numbers>

class Camera;

class TopDownCamera : public CameraWorkBase
{
public:
    void Initialize(Camera* camera)override;
    void Update() override;
    void Start(float height, const Vector3* target);

    void SetTarget(const Vector3* target);
    void SetHeight(float height);
	float GetHeight() const { return height_; }
    void SetActive(bool active);
	void SetPitch(float pitch) { pitch_ = pitch; }
	float GetPitch() const { return pitch_; }
	void SetYaw(float yaw) { yaw_ = yaw; }
	float GetYaw() const { return yaw_; }
	void SetOffset(const Vector3& offset) { offset_ = offset; } // カメラの位置オフセットを設定
	Vector3 GetOffset() const { return offset_; }
   


private:
    Camera* camera_ = nullptr;
    const Vector3* target_ = nullptr;                   // 追従するターゲット
    float height_ = 10.0f;                              // カメラの高さ
    float pitch_ = std::numbers::pi_v<float> / 2.0f;    // X軸角度(デフォルトで真下を向く)
	float yaw_ = 0.0f;                                  // Y軸角度
	Vector3 offset_ = {};                               // カメラの位置オフセット
    bool isActive_ = false;
};
