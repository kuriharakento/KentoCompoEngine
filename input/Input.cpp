#include "input/Input.h"
#include "base/WinApp.h"
#include "base/Logger.h"
#include "editor/SceneViewContext.h"

#include <cassert>
#include <cstring>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace KCE
{
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

// マウスボタンの数
constexpr int kMouseButtonCount = 3;
// キーボードのキー数
constexpr int kKeyboardKeyCount = 256;
// デフォルトのデッドゾーン値
constexpr float kDefaultDeadZone = 0.1f;
// ゲームパッドの最大接続数
constexpr DWORD kMaxGamepadCount = XUSER_MAX_COUNT;
// 振動停止時の値
constexpr WORD kVibrationOff = 0;
// キーボードバッファサイズ
constexpr int kMaxBufferSize = 256;

// シングルトンのインスタンス初期化
std::unique_ptr<Input> Input::instance_ = nullptr;

Input* Input::GetInstance()
{
    if (!instance_)
    {
		instance_ = std::make_unique<Input>();
    }
    return instance_.get();
}

Input::Input()
    : winApp_(nullptr), deadZone_(kDefaultDeadZone), isRecording_(false), isPlaying_(false), playIndex_(0)
{
    // ゲームパッド状態を初期化
    for (auto& gamepad : gamepads_) {
        ZeroMemory(&gamepad, sizeof(GamepadState));
        gamepad.isConnected = false;
    }
}

Input::~Input()
{
    Finalize();
}

void Input::Initialize(WinApp* winApp)
{
    winApp_ = winApp;
    HRESULT result;

    // DirectInputのインスタンス生成
    result = DirectInput8Create(
        winApp_->GetHInstance(),
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        reinterpret_cast<void**>(directInput_.GetAddressOf()),
        nullptr
    );
    assert(SUCCEEDED(result));

    // キーボードデバイスの作成
    result = directInput_->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), NULL);
    assert(SUCCEEDED(result));

    // データ形式の設定
    result = keyboard_->SetDataFormat(&c_dfDIKeyboard);
    assert(SUCCEEDED(result));

    // 排他制御の設定
    result = keyboard_->SetCooperativeLevel(winApp_->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    assert(SUCCEEDED(result));

    // ゲームパッドの初期化
    for (DWORD i = 0; i < kMaxGamepadCount; ++i)
    {
        gamepads_[i].isConnected = false;
        ZeroMemory(&gamepads_[i].vibration, sizeof(XINPUT_VIBRATION));
    }
}

void Input::Finalize()
{
    // キーボードデバイスの解放
    if (keyboard_)
    {
        keyboard_->Unacquire();
        keyboard_.Reset();
    }

    // DirectInputの解放
    if (directInput_)
    {
        directInput_.Reset();
    }

    // ゲームパッドの振動停止
    for (DWORD i = 0; i < kMaxGamepadCount; ++i)
    {
        SetVibration(i, kVibrationOff, kVibrationOff);
    }

    Logger::Log("Inputクラスの終了処理が完了しました。\n");
}

void Input::Update() {
    // 描画パス（前フレーム）で設定された補正値を適用し、バッファをクリア
    hasCorrection_ = hasNextCorrection_;
    mouseOffset_ = nextMouseOffset_;
    mouseSize_ = nextMouseSize_;
    hasNextCorrection_ = false;

    HRESULT result;

    // ウィンドウがアクティブかどうかを確認
    HWND hwnd = GetActiveWindow();
    if (hwnd != winApp_->GetHwnd()) {
        // ウィンドウが非アクティブの場合、マウスの移動量をリセット
        mouseDeltaX_ = 0.0f;
        mouseDeltaY_ = 0.0f;
        return;
    }

    // マウス固定の有効/無効を切り替え
    if (TriggerKey(DIK_F1)) {
        isMouseLockEnabled_ = !isMouseLockEnabled_;
        Logger::Log(isMouseLockEnabled_ ? "マウス固定: 有効\n" : "マウス固定: 無効\n");
    }

    // マウスの状態を更新
    {
        // マウス固定でカーソルを戻す中心。エディタでは Scene の画像の中心にする。
        // ウィンドウの中心だと、Scene が左上などにあるときに別のウィンドウの上へ戻してしまい、そこの UI が動いてしまう
        RECT rect;
        GetClientRect(hwnd, &rect);
        POINT center = {
            (rect.right - rect.left) / 2,
            (rect.bottom - rect.top) / 2
        };
        if (SceneViewContext::HasInstance() && SceneViewContext::GetInstance()->GetViewportRect().IsValid())
        {
            // Scene の矩形はスクリーン座標なので、クライアント座標に直す（0.5f は中心を取るための半分）
            const SceneViewRect& view = SceneViewContext::GetInstance()->GetViewportRect();
            POINT viewCenter = {
                static_cast<LONG>(view.x + view.width * 0.5f),
                static_cast<LONG>(view.y + view.height * 0.5f)
            };
            ScreenToClient(hwnd, &viewCenter);
            center = viewCenter;
        }

        // 現在のマウス座標を取得
        POINT mousePos;
        GetCursorPos(&mousePos);
        ScreenToClient(hwnd, &mousePos);

        mousePos_ = mousePos;

        // マウスの移動量を計算
        mouseDeltaX_ = static_cast<float>(mousePos.x - center.x);
        mouseDeltaY_ = static_cast<float>(mousePos.y - center.y);

        // マウスボタンの前フレーム状態を保存
        for (int i = 0; i < kMouseButtonCount; ++i)
        {
            mouseButtonsPre_[i] = mouseButtons_[i];
        }

        // マウスボタンの状態を取得
        mouseButtons_[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? 1 : 0; // 左ボタン
        mouseButtons_[1] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) ? 1 : 0; // 中ボタン
        mouseButtons_[2] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) ? 1 : 0; // 右ボタン

        // ロックを解除した瞬間に押しっぱなしのボタンを「押された瞬間」にしないよう、消す前の状態を残す
        memcpy(rawMouseButtons_, mouseButtons_, sizeof(mouseButtons_));

        // Scene の上で始めたマウス固定（デバッグカメラの右ドラッグなど）は、ボタンを離すまでゲームが持ち続ける。
        // 途中でカーソルが Scene の外へはみ出しても UI に渡さない。渡すと固定が止まり、カーソルの下の UI が動いてしまう
        bool anyRawButton = false;
        for (int i = 0; i < kMouseButtonCount; ++i)
        {
            anyRawButton = anyRawButton || rawMouseButtons_[i] != 0;
        }
        if (!anyRawButton || !isMouseLockEnabled_)
        {
            mouseLookOwnedByGame_ = false;
        }
        else if (!IsUICapturingMouse())
        {
            mouseLookOwnedByGame_ = true;
        }
        const bool uiCapturingMouse = IsUICapturingMouse() && !mouseLookOwnedByGame_;

        // ImGuiのウィンドウやギズモを操作している間はゲーム側にクリックを渡さない
        if (uiCapturingMouse || gameplayLocked_) {
            for (int i = 0; i < kMouseButtonCount; ++i) {
                mouseButtons_[i] = 0;
            }
        }

        // マウス固定が有効な場合、位置をウィンドウの中央にリセット。
        // ImGui のウィンドウを触っている間は戻さない。戻すとカーソルが中央へ引き戻され、UI が反応しなくなる
        if (isMouseLockEnabled_ && !uiCapturingMouse) {
            ClientToScreen(hwnd, &center);
            SetCursorPos(center.x, center.y);
        }
        else {
            // 固定無効時は移動量をリセット
            mouseDeltaX_ = 0.0f;
            mouseDeltaY_ = 0.0f;
        }

        // マウスの表示状態を更新
        if(preMouseVisible_ != isMouseVisible_)
        {
            ShowCursor(isMouseVisible_);
			preMouseVisible_ = isMouseVisible_;
        }
    }

    // キーボードの状態を更新
    {
        // 前回のキー情報を保存
        memcpy(keyPre_, key_, sizeof(key_));

        // キーボードのアクセス権を取得
        // 取れなければこのフレームはキー入力なしで進み、次のフレームでまた試す。
        // 取れるまで待つと、その間ゲームごと止まってカクつく
        result = keyboard_->Acquire();

        // キーボードの状態を取得
        result = keyboard_->GetDeviceState(sizeof(key_), key_);
        if (FAILED(result)) {
            // 取得失敗時はキー状態をクリア
            memset(key_, 0, sizeof(key_));
        }

        // エディタのテキスト入力中などにゲーム側が反応しないようにする。
        // これが無いと、名前の入力中に移動キーでキャラが動いてしまう。
        if (IsUICapturingKeyboard()) {
            memset(key_, 0, sizeof(key_));
        }

        // ロック前の状態を残しておく。カットシーンのスキップなど、
        // ロック中でも受け付けたいシステムの入力はこちらを読む
        memcpy(rawKeyPre_, rawKey_, sizeof(rawKey_));
        memcpy(rawKey_, key_, sizeof(key_));

        if (gameplayLocked_) {
            memset(key_, 0, sizeof(key_));
        }
    }

    // ゲームパッドの状態を更新（XInput）
    {
        for (DWORD i = 0; i < kMaxGamepadCount; ++i) {
            XINPUT_STATE state;
            ZeroMemory(&state, sizeof(XINPUT_STATE));
            DWORD dwResult = XInputGetState(i, &state);

            if (dwResult == ERROR_SUCCESS) {
                // 接続されている場合、状態を更新
                gamepads_[i].isConnected = true;
                gamepads_[i].prevState = gamepads_[i].state;
                gamepads_[i].state = state;
            }
            else {
                // 切断されている場合、状態をクリア
                gamepads_[i].isConnected = false;
                ZeroMemory(&gamepads_[i].state, sizeof(XINPUT_STATE));
                ZeroMemory(&gamepads_[i].prevState, sizeof(XINPUT_STATE));
            }
        }

        // ロック前のボタンを残してから、ロック中はゲームが読む状態を空にする
        for (DWORD i = 0; i < kMaxGamepadCount; ++i) {
            rawGamepadButtonsPre_[i] = rawGamepadButtons_[i];
            rawGamepadButtons_[i] = gamepads_[i].isConnected ? gamepads_[i].state.Gamepad.wButtons : 0;

            if (gameplayLocked_) {
                ZeroMemory(&gamepads_[i].state.Gamepad, sizeof(XINPUT_GAMEPAD));
                ZeroMemory(&gamepads_[i].prevState.Gamepad, sizeof(XINPUT_GAMEPAD));
            }
        }
    }

    // 入力の記録
    if (isRecording_) {
        for (const auto& [action, binding] : buttonMappings_) {
            if (binding.type == InputType::Keyboard) {
                if (PushKey(static_cast<BYTE>(binding.code))) {
                    recordedInputs_.emplace_back(action, binding.code);
                }
            }
            else if (binding.type == InputType::Gamepad) {
                // ゲームパッドインデックスとボタンコードを分離
                DWORD gamepadIndex = binding.code >> 16;
                DWORD buttonCode = binding.code & 0xFFFF;
                if (IsButtonPressed(gamepadIndex, buttonCode)) {
                    recordedInputs_.emplace_back(action, binding.code);
                }
            }
        }
    }

    // 入力の再生
    if (isPlaying_) {
        if (playIndex_ < recordedInputs_.size()) {
            Action action = recordedInputs_[playIndex_].first;
            DWORD code = recordedInputs_[playIndex_].second;
            auto it = actionCallbacks_.find(action);
            if (it != actionCallbacks_.end() && it->second) {
                it->second();
            }
            playIndex_++;
        }
        else {
            // 再生完了
            isPlaying_ = false;
            playIndex_ = 0;
            Logger::Log("入力の再生が完了しました。\n");
        }
    }

    // アクションの実行
    for (const auto& [action, binding] : buttonMappings_) {
        if (binding.type == InputType::Keyboard) {
            if (TriggerKey(static_cast<BYTE>(binding.code))) {
                auto it = actionCallbacks_.find(action);
                if (it != actionCallbacks_.end() && it->second) {
                    it->second();
                }
            }
        }
        else if (binding.type == InputType::Gamepad) {
            // ゲームパッドインデックスとボタンコードを分離
            DWORD gamepadIndex = binding.code >> 16;
            DWORD buttonCode = binding.code & 0xFFFF;
            if (IsButtonTriggered(gamepadIndex, buttonCode)) {
                auto it = actionCallbacks_.find(action);
                if (it != actionCallbacks_.end() && it->second) {
                    it->second();
                }
            }
        }
    }
}

bool Input::PushKey(BYTE keyNumber) const
{
    if (keyNumber >= kKeyboardKeyCount)
        return false;
    return key_[keyNumber] & 0x80;
}

bool Input::TriggerKey(BYTE keyNumber) const
{
    if (keyNumber >= kKeyboardKeyCount)
        return false;
    return (key_[keyNumber] & 0x80) && !(keyPre_[keyNumber] & 0x80);
}

bool Input::ReleaseTrigger(BYTE keyNumber) const
{
    if (keyNumber >= kKeyboardKeyCount)
        return false;
    return !(key_[keyNumber] & 0x80) && (keyPre_[keyNumber] & 0x80);
}

bool Input::ReleaseButton(DWORD gamepadIndex, DWORD buttonCode) const
{
    if (gamepadIndex >= kMaxGamepadCount || !gamepads_[gamepadIndex].isConnected)
        return false;

    const XINPUT_STATE& currentState = gamepads_[gamepadIndex].state;
    const XINPUT_STATE& prevState = gamepads_[gamepadIndex].prevState;

    bool wasPressed = (prevState.Gamepad.wButtons & buttonCode) != 0;
    bool isPressed = (currentState.Gamepad.wButtons & buttonCode) != 0;

    return wasPressed && !isPressed;
}

void Input::SetDeadZone(float deadZone)
{
    deadZone_ = deadZone;
}

void Input::SetVibration(DWORD gamepadIndex, WORD leftMotor, WORD rightMotor)
{
    if (gamepadIndex >= kMaxGamepadCount)
        return;

    if (gamepads_[gamepadIndex].isConnected)
    {
        XINPUT_VIBRATION vibration;
        ZeroMemory(&vibration, sizeof(XINPUT_VIBRATION));
        vibration.wLeftMotorSpeed = leftMotor;
        vibration.wRightMotorSpeed = rightMotor;
        XInputSetState(gamepadIndex, &vibration);
        gamepads_[gamepadIndex].vibration = vibration;
    }
}

void Input::RemapButton(Action action, InputType type, DWORD code)
{
    buttonMappings_[action] = InputBinding{ type, code };
}

void Input::BindAction(Action action, std::function<void()> callback)
{
    actionCallbacks_[action] = callback;
}

void Input::StartRecording()
{
    isRecording_ = true;
    recordedInputs_.clear();
    Logger::Log("入力の記録を開始しました。\n");
}

void Input::StopRecording()
{
    isRecording_ = false;
    Logger::Log("入力の記録を停止しました。\n");
}

void Input::PlayRecording()
{
    if (!recordedInputs_.empty())
    {
        isPlaying_ = true;
        playIndex_ = 0;
        Logger::Log("入力の再生を開始しました。\n");
    }
}

bool Input::IsButtonPressed(DWORD gamepadIndex, DWORD buttonCode) const
{
    if (gamepadIndex >= kMaxGamepadCount || !gamepads_[gamepadIndex].isConnected)
        return false;

    WORD buttons = gamepads_[gamepadIndex].state.Gamepad.wButtons;
    return (buttons & buttonCode) != 0;
}

bool Input::IsButtonTriggered(DWORD gamepadIndex, DWORD buttonCode) const
{
    if (gamepadIndex >= kMaxGamepadCount || !gamepads_[gamepadIndex].isConnected)
        return false;

    const XINPUT_STATE& currentState = gamepads_[gamepadIndex].state;
    const XINPUT_STATE& prevState = gamepads_[gamepadIndex].prevState;

    bool wasPressed = (prevState.Gamepad.wButtons & buttonCode) != 0;
    bool isPressed = (currentState.Gamepad.wButtons & buttonCode) != 0;

    return isPressed && !wasPressed;
}

bool Input::IsMouseButtonPressed(int button) const
{
	if (button < 0 || button >= kMouseButtonCount)
		return false;
	return mouseButtons_[button] == 1;
}

bool Input::IsMouseButtonTriggered(int button) const
{
	if (button < 0 || button >= kMouseButtonCount)
		return false;
	return mouseButtons_[button] == 1 && mouseButtonsPre_[button] == 0;
}

bool Input::IsMouseButtonReleased(int button) const
{
	if (button < 0 || button >= kMouseButtonCount)
		return false;
	return mouseButtons_[button] == 0 && mouseButtonsPre_[button] == 1;
}

float Input::GetMouseDeltaX() const
{
	return mouseDeltaX_;
}

float Input::GetMouseDeltaY() const
{
	return mouseDeltaY_;
}

float Input::GetMouseX() const
{
	float realX = static_cast<float>(mousePos_.x);
	if (hasCorrection_ && mouseSize_.x > 0.0f)
	{
		float relativeX = realX - mouseOffset_.x;
		return relativeX * (static_cast<float>(winApp_->GetClientWidth()) / mouseSize_.x);
	}
	return realX;
}

float Input::GetMouseY() const
{
	float realY = static_cast<float>(mousePos_.y);
	if (hasCorrection_ && mouseSize_.y > 0.0f)
	{
		float relativeY = realY - mouseOffset_.y;
		return relativeY * (static_cast<float>(winApp_->GetClientHeight()) / mouseSize_.y);
	}
	return realY;
}

Vector2 Input::GetMousePosition() const
{
	return Vector2{ GetMouseX(), GetMouseY() };
}

bool Input::IsUICapturingKeyboard() const
{
#ifdef USE_IMGUI
	if (!isUICaptureEnabled_ || !ImGui::GetCurrentContext())
	{
		return false;
	}
	return ImGui::GetIO().WantCaptureKeyboard;
#else
	return false;
#endif
}

bool Input::IsUICapturingMouse() const
{
#ifdef USE_IMGUI
	if (!isUICaptureEnabled_ || !ImGui::GetCurrentContext())
	{
		return false;
	}
	// シーン画像も ImGui のウィンドウの中なので、上にあるだけで WantCaptureMouse が立つ。
	// そこはゲームの画面なので、デバッグカメラの右ドラッグなどが届くようにマウスはゲームへ渡す
	if (SceneViewContext::HasInstance() && SceneViewContext::GetInstance()->IsHovered())
	{
		return false;
	}
	return ImGui::GetIO().WantCaptureMouse;
#else
	return false;
#endif
}

void Input::SetGameplayLocked(bool locked)
{
	if (gameplayLocked_ && !locked)
	{
		// ロック中に押しっぱなしだったキーが、解除した瞬間に
		// 「押された瞬間」と判定されないよう、前フレームの状態を実際の状態で埋める
		memcpy(keyPre_, rawKey_, sizeof(rawKey_));

		// ゲームパッドとマウスは次の Update で「今の状態」が前フレームへ写されるので、
		// 今の状態のほうを実際の値に戻しておく。これが無いと START でスキップした直後に
		// START を押しっぱなしだと、ゲーム側にポーズの入力として届いてしまう
		for (DWORD i = 0; i < kMaxGamepadCount; ++i)
		{
			gamepads_[i].state.Gamepad.wButtons = rawGamepadButtons_[i];
		}
		memcpy(mouseButtons_, rawMouseButtons_, sizeof(mouseButtons_));
	}
	gameplayLocked_ = locked;
}

bool Input::TriggerKeyRaw(BYTE keyNumber) const
{
	return (rawKey_[keyNumber] & 0x80) && !(rawKeyPre_[keyNumber] & 0x80);
}

bool Input::IsButtonTriggeredRaw(DWORD gamepadIndex, DWORD buttonCode) const
{
	if (gamepadIndex >= kMaxGamepadCount)
	{
		return false;
	}
	return (rawGamepadButtons_[gamepadIndex] & buttonCode) && !(rawGamepadButtonsPre_[gamepadIndex] & buttonCode);
}
} // namespace KCE
