#include "Camera.h"

Camera::Camera()
	: transform_({ 1.0f,1.0f,1.0f }, { 0.0f,0.0f,0.0f }, { 0.0f,4.0f,-10.0f })
	, fovY_(0.45f)
	, aspectRatio_(float(WinApp::kClientWidth) / float(WinApp::kClientHeight))
	, nearClip_(0.1f)
	, farClip_(300.0f)
	, worldMatrix_(MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate))
	, viewMatrix_(Inverse(worldMatrix_))
	, projectionMatrix_(MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_))
	, viewProjectionMatrix_(viewMatrix_* projectionMatrix_)
{
}

void Camera::Update()
{
	// シェイクタイマーが有効ならオフセットを更新
	if (shakeTimer_ > 0.0f) {
		shakeTimer_ -= 1.0f / 60.0f; // フレーム更新（60FPS想定）
		float progress = 1.0f - (shakeTimer_ / shakeDuration_);
		float damping = (1.0f - progress); // 減衰計算
		shakeOffset_.x = ((rand() % 200 - 100) / 100.0f) * shakeIntensity_ * damping;
		shakeOffset_.y = ((rand() % 200 - 100) / 100.0f) * shakeIntensity_ * damping;
		shakeOffset_.z = ((rand() % 200 - 100) / 100.0f) * shakeIntensity_ * damping;
	} else {
		shakeOffset_ = { 0.0f, 0.0f, 0.0f }; // シェイク終了時リセット
	}

	//ワールド行列の更新
	worldMatrix_ = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate + shakeOffset_);
	//ビュー行列の更新
	viewMatrix_ = Inverse(worldMatrix_);
	//透視投影行列の更新
	projectionMatrix_ = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
	//ビュープロジェクション行列の更新
	viewProjectionMatrix_ = viewMatrix_ * projectionMatrix_;

}

void Camera::StartShake(float intensity, float duration)
{
	shakeIntensity_ = intensity;
	shakeDuration_ = duration;
	shakeTimer_ = duration;
}