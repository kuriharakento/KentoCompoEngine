#include "Camera.h"
#include "DirectXCommon.h"

// フレームレート（FPS）
constexpr float kFrameRate = 60.0f;
// 1フレームあたりの時間（秒）
constexpr float kDeltaTime = 1.0f / kFrameRate;
// シェイクのランダム範囲
constexpr int kShakeRandomRange = 200;
// シェイクのランダムオフセット
constexpr int kShakeRandomOffset = 100;
// シェイクの正規化係数
constexpr float kShakeNormalizeFactor = 100.0f;

Camera::Camera()
	: transform_({ 1.0f,1.0f,1.0f }, { 0.0f,0.0f,0.0f }, { 0.0f,4.0f,-10.0f })
	, fovY_(0.45f)
	, aspectRatio_(float(WinApp::kClientWidth) / float(WinApp::kClientHeight))
	, nearClip_(0.1f)
	, farClip_(3000.0f)
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
		shakeTimer_ -= kDeltaTime;
		float progress = 1.0f - (shakeTimer_ / shakeDuration_);
		// 減衰計算
		float damping = (1.0f - progress);
		// ランダムなオフセットを計算
		shakeOffset_.x = ((rand() % kShakeRandomRange - kShakeRandomOffset) / kShakeNormalizeFactor) * shakeIntensity_ * damping;
		shakeOffset_.y = ((rand() % kShakeRandomRange - kShakeRandomOffset) / kShakeNormalizeFactor) * shakeIntensity_ * damping;
		shakeOffset_.z = ((rand() % kShakeRandomRange - kShakeRandomOffset) / kShakeNormalizeFactor) * shakeIntensity_ * damping;
	} else {
		// シェイク終了時リセット
		shakeOffset_ = { 0.0f, 0.0f, 0.0f };
	}

	// ワールド行列の更新
	worldMatrix_ = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate + shakeOffset_);
	// ビュー行列の更新
	viewMatrix_ = Inverse(worldMatrix_);
	// 透視投影行列の更新
	projectionMatrix_ = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
	// ビュープロジェクション行列の更新
	viewProjectionMatrix_ = viewMatrix_ * projectionMatrix_;

	// GPU定数バッファの更新
	if (constantData_)
	{
		constantData_->view = viewMatrix_;
		constantData_->projection = projectionMatrix_;
		constantData_->eye = transform_.translate + shakeOffset_;
		constantData_->padding = 0.0f;
	}
}

void Camera::StartShake(float intensity, float duration)
{
	shakeIntensity_ = intensity;
	shakeDuration_ = duration;
	shakeTimer_ = duration;
}

void Camera::InitializeConstantBuffer(DirectXCommon* dxCommon)
{
	if (constantBuffer_) return; // 既に初期化済み

	constantBuffer_ = dxCommon->CreateBufferResource(sizeof(CameraGPUData));
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantData_));

	// 初期値を設定
	if (constantData_)
	{
		constantData_->view = viewMatrix_;
		constantData_->projection = projectionMatrix_;
		constantData_->eye = transform_.translate;
		constantData_->padding = 0.0f;
	}
}

D3D12_GPU_VIRTUAL_ADDRESS Camera::GetConstantBufferAddress() const
{
	if (constantBuffer_)
	{
		return constantBuffer_->GetGPUVirtualAddress();
	}
	return 0;
}