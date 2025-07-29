#include "TopDownCamera.h"

#include "base/Camera.h"
#include "imgui/imgui_internal.h"
#include "math/MathUtils.h"

void TopDownCamera::Initialize(Camera* camera)
{
    camera_ = camera;
}

void TopDownCamera::Update()
{
#ifdef _DEBUG
	ImGui::Begin("TopDownCamera Settings");
	Vector3 cameraPos = camera_->GetTranslate();
	ImGui::DragFloat3("Camera Position", &cameraPos.x, 0.1f);
	camera_->SetTranslate(cameraPos);
	Vector3 cameraRotate = camera_->GetRotate();
	ImGui::DragFloat3("Camera Rotate", &cameraRotate.x, 0.1f);
	camera_->SetRotate(cameraRotate);
	ImGui::DragFloat("Camera Height", &height_, 0.1f);
	ImGui::DragFloat("Camera Pitch", &pitch_, 0.1f);
	ImGui::DragFloat("Camera Yaw", &yaw_, 0.1f);
	ImGui::DragFloat3("Camera Offset", &offset_.x, 0.1f);
	ImGui::End();
#endif

    if (!camera_ || !target_ || !isActive_) return;

    // ターゲットの位置を基準にカメラの位置を計算
    Vector3 targetPos = *target_;
    Vector3 targetCameraPos = targetPos + Vector3(0.0f, height_, 0.0f);

	//イージングを使用してカメラの位置を滑らかに移動
	Vector3 currentPosition = MathUtils::Lerp(camera_->GetTranslate(), targetCameraPos, 0.1f);

    // カメラの位置を設定
    camera_->SetTranslate(currentPosition + offset_);

	//カメラの向きを真下に向ける
	camera_->SetRotate(Vector3(pitch_, yaw_, 0.0f));
}

void TopDownCamera::Start(float height, const Vector3* target)
{
	height_ = height;
	target_ = target;
	isActive_ = true;
}

void TopDownCamera::SetTarget(const Vector3* target)
{
    target_ = target;
}

void TopDownCamera::SetHeight(float height)
{
    height_ = height;
}

void TopDownCamera::SetActive(bool active)
{
    isActive_ = active;
}
