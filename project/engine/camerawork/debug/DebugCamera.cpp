#include "DebugCamera.h"
#include "math/MathUtils.h"
#include <algorithm>
#include <DirectXMath.h>

// system
#include "input/Input.h"

#ifdef _DEBUG
#include "imgui/imgui.h"
#endif

void DebugCamera::Initialize(Camera* camera)
{
    camera_ = camera;
    isActive_ = false;
}

void DebugCamera::Start(const Vector3& initialPosition, const Vector3& initialRotation)
{
    // カメラの初期状態を設定
    camera_->SetTranslate(initialPosition);
    camera_->SetRotate(initialRotation);

    // 回転状態を初期化
    yaw_ = initialRotation.y;
    pitch_ = initialRotation.x;

    isActive_ = true;
}

void DebugCamera::Update()
{
    if (!isActive_ || !camera_) return;

    UpdateSpeedControl();
    UpdateMovement();
    UpdateMouseLook();

#ifdef _DEBUG
    if (showDebugUI_)
    {
        DrawDebugUI();
    }

    // Tabキーでデバッグ画面の表示切り替え
    bool currentTabPressed = Input::GetInstance()->TriggerKey(DIK_TAB);
    if (currentTabPressed && !prevTabPressed_)
    {
        showDebugUI_ = !showDebugUI_;
    }
    prevTabPressed_ = currentTabPressed;
#endif
}

void DebugCamera::UpdateMovement()
{
    Vector3 currentPos = camera_->GetTranslate();
    Vector3 moveDirection = { 0.0f, 0.0f, 0.0f };

    float deltaTime = 0.016f; // 60FPS想定
    float currentSpeed = moveSpeed_ * speedMultiplier_ * deltaTime;

    // WASD移動
    if (Input::GetInstance()->PushKey(DIK_W))
    {
        moveDirection = moveDirection + GetForwardVector();
    }
    if (Input::GetInstance()->PushKey(DIK_S))
    {
        moveDirection = moveDirection + GetForwardVector() * -1.0f;
    }
    if (Input::GetInstance()->PushKey(DIK_A))
    {
        moveDirection = moveDirection + GetRightVector() * -1.0f;
    }
    if (Input::GetInstance()->PushKey(DIK_D))
    {
        moveDirection = moveDirection + GetRightVector();
    }

    // 上下移動（Space/Shift）
    if (Input::GetInstance()->PushKey(DIK_SPACE))
    {
        moveDirection.y += 1.0f;
    }
    if (Input::GetInstance()->PushKey(DIK_LSHIFT))
    {
        moveDirection.y -= 1.0f;
    }

    // 移動方向を正規化して速度を適用
    if (moveDirection.x != 0.0f || moveDirection.y != 0.0f || moveDirection.z != 0.0f)
    {
        moveDirection.Normalize();
        currentPos = currentPos + moveDirection * currentSpeed;
        camera_->SetTranslate(currentPos);
    }
}

void DebugCamera::UpdateMouseLook()
{
    Input::GetInstance()->SetMouseLockEnabled(Input::GetInstance()->IsMouseButtonPressed(2));
	Input::GetInstance()->SetMouseVisible(!Input::GetInstance()->IsMouseButtonPressed(2));

    // マウスの移動量を取得
    float deltaX = Input::GetInstance()->GetMouseDeltaX();
    float deltaY = Input::GetInstance()->GetMouseDeltaY();

    // マウスの移動量に基づいてカメラの角度を更新
    yaw_ += deltaX * mouseSensitivity_;
    pitch_+= deltaY * mouseSensitivity_;

    // ピッチ角を制限
    const float pitchMin = -80.0f;    // 水平
    const float pitchMax = 80.0f;   // 真上にならない範囲
	pitch_ = std::clamp(pitch_, pitchMin, pitchMax);

	float radYaw = DirectX::XMConvertToRadians(yaw_);
	float radPitch = DirectX::XMConvertToRadians(pitch_);

    // 回転を適用
    Vector3 rotation = { radPitch, radYaw, 0.0f };
    camera_->SetRotate(rotation);
}

void DebugCamera::UpdateSpeedControl()
{
    
}

void DebugCamera::Stop()
{
    isActive_ = false;
}

void DebugCamera::Reset()
{
    if (!camera_) return;

    camera_->SetTranslate(Vector3());
    camera_->SetRotate(Vector3());

    yaw_ = 0.0f;
    pitch_ = 0.0f;
    speedMultiplier_ = 1.0f;
}

void DebugCamera::FocusOnTarget(const Vector3& target)
{
    if (!camera_) return;

    Vector3 currentPos = camera_->GetTranslate();
    Vector3 rotation = MathUtils::CalculateDirectionToTarget(currentPos, target);

    camera_->SetRotate(rotation);
    yaw_ = rotation.y;
    pitch_ = rotation.x;
}

Vector3 DebugCamera::GetForwardVector() const
{
    float radYaw = DirectX::XMConvertToRadians(yaw_);
    float radPitch = DirectX::XMConvertToRadians(pitch_);

    return {
        sin(radYaw) * cos(radPitch),
        -sin(radPitch),
        cos(radYaw) * cos(radPitch)
    };
}

Vector3 DebugCamera::GetRightVector() const
{
    float radYaw = DirectX::XMConvertToRadians(yaw_);

    return {
        cos(radYaw),
        0.0f,
        -sin(radYaw)
    };
}

Vector3 DebugCamera::GetUpVector() const
{
    return { 0.0f, 1.0f, 0.0f }; // 固定で上方向
}

#ifdef _DEBUG
void DebugCamera::DrawDebugUI()
{
    ImGui::Begin("Debug Camera", &showDebugUI_);

    // 現在の位置・回転情報
    Vector3 pos = camera_->GetTranslate();
    Vector3 rot = camera_->GetRotate();

    ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
    ImGui::Text("Rotation: (%.2f, %.2f, %.2f)", rot.x, rot.y, rot.z);

    ImGui::Separator();

    // 移動設定
    ImGui::SliderFloat("Move Speed", &moveSpeed_, 0.1f, 20.0f);
    ImGui::SliderFloat("Mouse Sensitivity", &mouseSensitivity_, 0.01f, 1.0f);
    ImGui::Text("Speed Multiplier: %.2f", speedMultiplier_);

    ImGui::Separator();

    ImGui::Separator();

    // 操作説明
    ImGui::Text("Controls:");
    ImGui::Text("WASD: Move");
    ImGui::Text("Space/Shift: Up/Down");
	ImGui::Text("Mouse : Right Click to look around");
    ImGui::Text("Tab: Toggle this panel");

    ImGui::Separator();

    // 操作ボタン
    if (ImGui::Button("Reset Camera"))
    {
        Reset();
    }

    ImGui::End();
}
#endif