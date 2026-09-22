#include "DebugCamera.h"
#include "math/MathUtils.h"
#include <algorithm>
#include <numbers>
#include <DirectXMath.h>

// system
#include "base/Camera.h"
#include "input/Input.h"
#include "manager/scene/CameraManager.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "editor/SceneViewContext.h"
#include "manager/editor/DebugUIManager.h"

#endif

namespace KCE
{

// 60FPS想定のフレームデルタタイム
constexpr float kFrameDeltaTime = 0.016f;
// ピッチ制限（ラジアン。角度はラジアンで持つので、80 度をラジアンにしておく）
constexpr float kPitchLimitMax = 80.0f * std::numbers::pi_v<float> / 180.0f;
constexpr float kPitchLimitMin = -kPitchLimitMax;

void DebugCamera::Initialize(Camera* camera)
{
    camera_ = camera;
    isActive_ = false;

#ifdef USE_IMGUI
    DebugUIManager::GetInstance()->RegisterSettingsPage(this, "カメラ", "デバッグカメラ", [this]() { this->DrawImGui(); });
#endif
}

DebugCamera::~DebugCamera()
{
#ifdef USE_IMGUI
    if (DebugUIManager::HasInstance())
    {
        DebugUIManager::GetInstance()->Unregister(this);
    }
#endif
}

void DebugCamera::SetEnabled(bool enabled)
{
    if (enabled == isActive_ || !camera_ || !cameraManager_)
    {
        return;
    }
    if (!enabled)
    {
        Stop();
        return;
    }

    // 今映っているカメラから始める。差し替える前なので GetActiveCamera は本来のカメラを返す
    if (const Camera* source = cameraManager_->GetActiveCamera())
    {
        camera_->SetTranslate(source->GetTranslate());
        camera_->SetRotateQuaternion(source->GetRotateQuaternion());
        camera_->SetFovY(source->GetFovY());
    }
    const Vector3 euler = camera_->GetRotateQuaternion().ToEuler();
    pitch_ = euler.x;
    yaw_ = euler.y;

    cameraManager_->SetViewCameraOverride(camera_);
    isActive_ = true;
}

void DebugCamera::Update()
{
    // オフの間も切り替えだけは受け付ける。Raw なのでカットシーンのロック中でも効く
    if (Input::GetInstance()->TriggerKeyRaw(DIK_F9))
    {
        SetEnabled(!isActive_);
    }

    if (!isActive_ || !camera_) return;

    // 右ドラッグしている間だけカメラに触る。
    // 触っていない間も回転を書くと、シーンやカットシーンが動かしたカメラを毎フレーム戻してしまう
    const bool rightPressed = Input::GetInstance()->IsMouseButtonPressed(2);
    if (!dragging_)
    {
        if (!rightPressed || !IsMouseOverSceneView())
        {
            return;
        }
        // 掴んだ瞬間に今の向きから始める。前回離したときの角度を使うと、他で動かされた分だけ向きが飛ぶ
        const Vector3 euler = camera_->GetRotateQuaternion().ToEuler();
        pitch_ = euler.x;
        yaw_ = euler.y;
        dragging_ = true;
        Input::GetInstance()->SetMouseLockEnabled(true);
        Input::GetInstance()->SetMouseVisible(false);
    }
    else if (!rightPressed)
    {
        // 離したらマウスを戻す。戻すのはこの1回だけで、ふだんはゲーム側の設定に触らない
        dragging_ = false;
        Input::GetInstance()->SetMouseLockEnabled(false);
        Input::GetInstance()->SetMouseVisible(true);
        return;
    }

    // 各種更新処理を実行
    UpdateMouseLook();
    UpdateMovement();
}

bool DebugCamera::IsMouseOverSceneView() const
{
#ifdef USE_IMGUI
    // Scene の上で掴んだときだけ動かす。他のウィンドウで右クリックしてもカメラが動かないように
    if (!SceneViewContext::HasInstance())
    {
        return true;
    }
    const SceneViewRect& rect = SceneViewContext::GetInstance()->GetViewportRect();
    if (!rect.IsValid())
    {
        return true;
    }
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    return mouse.x >= rect.x && mouse.x <= rect.x + rect.width && mouse.y >= rect.y && mouse.y <= rect.y + rect.height;
#else
    return true;
#endif
}

void DebugCamera::UpdateMovement()
{

    Vector3 currentPos = camera_->GetTranslate();
    Vector3 moveDirection = { 0.0f, 0.0f, 0.0f };

    // 現在フレームの移動速度を計算
    float currentSpeed = moveSpeed_ * speedMultiplier_ * kFrameDeltaTime;

    // WASD移動（前後左右）
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

    // 上下移動（Space: 上昇、LShift: 下降）
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
        moveDirection.NormalizeSelf();
        currentPos = currentPos + moveDirection * currentSpeed;
        camera_->SetTranslate(currentPos);
    }
}

void DebugCamera::UpdateMouseLook()
{
    // マウスの移動量を取得
    float deltaX = Input::GetInstance()->GetMouseDeltaX();
    float deltaY = Input::GetInstance()->GetMouseDeltaY();

    // マウスの移動量に基づいてカメラの角度を更新
    yaw_ += deltaX * mouseSensitivity_;
    pitch_+= deltaY * mouseSensitivity_;

    // ピッチ角を制限（真上・真下を向かないように）
	pitch_ = std::clamp(pitch_, kPitchLimitMin, kPitchLimitMax);

    // 回転を適用
	camera_->SetRotate({ pitch_, yaw_, 0.0f });
}

void DebugCamera::Stop()
{
    isActive_ = false;
    if (cameraManager_)
    {
        cameraManager_->ClearViewCameraOverride();
    }
    // 掴んだまま止めると、マウスが隠れて固定されたままになる
    if (dragging_)
    {
        dragging_ = false;
        Input::GetInstance()->SetMouseLockEnabled(false);
        Input::GetInstance()->SetMouseVisible(true);
    }
}

void DebugCamera::Reset()
{
    if (!camera_) return;

    // カメラの位置と回転を原点にリセット
    camera_->SetTranslate(Vector3());
    camera_->SetRotate(Vector3());

    // 回転状態をリセット
    yaw_ = 0.0f;
    pitch_ = 0.0f;
    speedMultiplier_ = 1.0f;
}

void DebugCamera::FocusOnTarget(const Vector3& target)
{
    if (!camera_) return;

    // 現在位置からターゲットへの方向を計算
    Vector3 currentPos = camera_->GetTranslate();
    Vector3 rotation = MathUtils::CalculateDirectionToTarget(currentPos, target);

    // カメラの回転を設定
    camera_->SetRotate(rotation);
    yaw_ = rotation.y;
    pitch_ = rotation.x;
}

Vector3 DebugCamera::GetForwardVector() const
{
    return {
        sin(yaw_) * cos(pitch_),
        -sin(pitch_),
        cos(yaw_) * cos(pitch_)
    };
}

Vector3 DebugCamera::GetRightVector() const
{
    // ヨーから右方ベクトルを計算（Y軸回転のみ考慮）
    return {
        cos(yaw_),
        0.0f,
        -sin(yaw_)
    };
}

Vector3 DebugCamera::GetUpVector() const
{
    // 固定で上方向を返す
    return { 0.0f, 1.0f, 0.0f };
}


void DebugCamera::DrawImGui()
{
#ifdef USE_IMGUI

    // オフにすると、元のカメラの映像に戻って、右クリックもゲームに渡る
    bool active = isActive_;
    if (ImGui::Checkbox("有効 (F9)（Scene の上で右ドラッグ + WASD）", &active))
    {
        SetEnabled(active);
    }
    ImGui::Separator();

    // 現在の位置・回転情報
    Vector3 pos = camera_->GetTranslate();
    Vector3 rot = camera_->GetRotate();

    ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
    ImGui::Text("Rotation: (%.2f, %.2f, %.2f)", rot.x, rot.y, rot.z);

    ImGui::Separator();

    // 移動設定
    ImGui::SliderFloat("移動速度", &moveSpeed_, 0.1f, 20.0f);
    ImGui::SliderFloat("マウス感度", &mouseSensitivity_, 0.01f, 1.0f);
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
    if (ImGui::Button("カメラをリセット"))
    {
        Reset();
    }
#endif
}
} // namespace KCE
