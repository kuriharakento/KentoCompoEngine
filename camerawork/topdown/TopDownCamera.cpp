#include "TopDownCamera.h"

#include "base/Camera.h"
#include "imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"
#include "math/MathUtils.h"

namespace KCE
{
// カメラ補間速度（イージング係数）
constexpr float kCameraLerpSpeed = 0.1f;

void TopDownCamera::Initialize(Camera* camera)
{
    camera_ = camera;

#ifdef USE_IMGUI
    DebugUIManager::GetInstance()->RegisterSettingsPage(this, "カメラ", "見下ろしカメラ", [this]() { this->DrawImGui(); });
#endif
}

TopDownCamera::~TopDownCamera()
{
#ifdef USE_IMGUI
    if (DebugUIManager::HasInstance())
    {
        DebugUIManager::GetInstance()->Unregister(this);
    }
#endif
}

void TopDownCamera::Update()
{

    if (!camera_ || !target_ || !isActive_) return;

    // ターゲットの位置を基準にカメラの目標位置を計算
    Vector3 targetPos = *target_;
    Vector3 targetCameraPos = targetPos + Vector3(0.0f, height_, 0.0f);

    // 現在のカメラ位置からオフセットを差し引いて「生の位置」を取得
    Vector3 currentWorld = camera_->GetTranslate() - offset_;

	// イージングを使用してカメラの位置を滑らかに移動
	Vector3 newWorld = MathUtils::Lerp(currentWorld, targetCameraPos, kCameraLerpSpeed);

    // オフセットを加えて最終位置を設定
    camera_->SetTranslate(newWorld + offset_);

	// カメラの向きを設定（真下を向く）
	camera_->SetRotate(Vector3(pitch_, yaw_, 0.0f));
}

void TopDownCamera::Start(float height, const Vector3* target)
{
    // パラメータを設定
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

#ifdef USE_IMGUI
void TopDownCamera::DrawImGui()
{
	Vector3 cameraPos = camera_->GetTranslate();
	ImGui::DragFloat3("カメラ位置", &cameraPos.x, 0.1f);
	camera_->SetTranslate(cameraPos);
	Vector3 cameraRotate = camera_->GetRotate();
	ImGui::DragFloat3("カメラ回転", &cameraRotate.x, 0.1f);
	camera_->SetRotate(cameraRotate);
	ImGui::DragFloat("高さ", &height_, 0.1f);
	ImGui::DragFloat("ピッチ", &pitch_, 0.1f);
	ImGui::DragFloat("ヨー", &yaw_, 0.1f);
	ImGui::DragFloat3("オフセット", &offset_.x, 0.1f);
}
#endif
} // namespace KCE
