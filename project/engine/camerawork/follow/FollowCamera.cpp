#include "FollowCamera.h"

#include <algorithm>
#include <DirectXMath.h>

#include "input/Input.h"
#include "math/MathUtils.h"

void FollowCamera::Initialize(Camera* camera)
{
	camera_ = camera;
}

void FollowCamera::Start(const Vector3* target, float distance, float sensitivity)
{
    target_ = target;
    distance_ = distance;
    sensitivity_ = sensitivity;
    yaw_ = 0.0f;
    pitch_ = 0.0f;
    isActive_ = true;
}

void FollowCamera::Update()
{
    if (!isActive_ || !target_) return;

    // マウスの移動量を取得
    float deltaX = Input::GetInstance()->GetMouseDeltaX();
    float deltaY = Input::GetInstance()->GetMouseDeltaY();

    // マウスの移動量に基づいてカメラの角度を更新
    yaw_ += deltaX * sensitivity_;
    pitch_ -= deltaY * sensitivity_;

    // ピッチ角を制限（水平から真上にならない範囲に制御）
    const float pitchMin = -5.0f;    // 水平
    const float pitchMax = 50.0f;   // 真上にならない範囲
    pitch_ = std::clamp(pitch_, pitchMin, pitchMax);

    // カメラの位置を計算
    float radYaw = DirectX::XMConvertToRadians(yaw_);
    float radPitch = DirectX::XMConvertToRadians(pitch_);
    Vector3 offset = {
        distance_ * cos(radPitch) * sin(radYaw),
        distance_ * sin(radPitch),
        distance_ * cos(radPitch) * cos(radYaw)
    };

    // カメラの位置をターゲットの周囲に設定
    Vector3 cameraPos = *target_ + offset;
    camera_->SetTranslate(cameraPos);

    // カメラがターゲットを向くように設定
    camera_->SetRotate(MathUtils::CalculateDirectionToTarget(cameraPos, *target_));

}
