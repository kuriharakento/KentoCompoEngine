#pragma once

#define DIRECTINPUT_VERSION 0x0800 //DirectInputのバージョン指定
#include <dinput.h>

#include <Xinput.h>
#pragma comment(lib, "Xinput.lib")

#include <wrl.h>
#include <array>
#include <unordered_map>
#include <vector>
#include <functional>



// 前方宣言
class WinApp;

// アクションの列挙型
enum class Action {
    MoveLeft,
    MoveRight,
    Jump,
    Shoot,
    // 他のアクションを追加...
};

// 入力デバイスのタイプ
enum class InputType {
    Keyboard,
    Gamepad
};

class Input {
public:
    // シングルトンの取得
    static Input* GetInstance();

    // 初期化
    void Initialize(WinApp* winApp);

    // 終了処理
    void Finalize();

    // 更新
    void Update();

    // キーの押下チェック
    bool PushKey(BYTE keyNumber) const;

    // キーのトリガーチェック
    bool TriggerKey(BYTE keyNumber) const;

    // キーのリリースチェック
    bool ReleaseTrigger(BYTE keyNumber) const;

    // ゲームパッドボタンのリリースチェック
    bool ReleaseButton(DWORD gamepadIndex, DWORD buttonCode) const;

    // デッドゾーンの設定
    void SetDeadZone(float deadZone);

    // ゲームパッドの振動設定
    void SetVibration(DWORD gamepadIndex, WORD leftMotor, WORD rightMotor);

    // ボタンのリマッピング
    void RemapButton(Action action, InputType type, DWORD code);

    // アクションのバインディング
    void BindAction(Action action, std::function<void()> callback);

    // 入力の記録開始
    void StartRecording();

    // 入力の記録停止
    void StopRecording();

    // 入力の再生開始
    void PlayRecording();

	//ボタンの押下チェック
    bool IsButtonPressed(DWORD gamepadIndex, DWORD buttonCode) const;

	// ボタンのトリガーチェック
    bool IsButtonTriggered(DWORD gamepadIndex, DWORD buttonCode) const;

    // マウスのボタン押下チェック
    bool IsMouseButtonPressed(int button) const;

    // マウスのボタンのトリガーチェック
    bool IsMouseButtonTriggered(int button) const;

    // マウスのボタンのリリースチェック
    bool IsMouseButtonReleased(int button) const;

    // マウスの移動量を取得
    float GetMouseDeltaX() const;
    float GetMouseDeltaY() const;

	//マウスの座標を取得
	float GetMouseX() const;
	float GetMouseY() const;

    // マウスの固定状態を設定
	void SetMouseLockEnabled(bool enabled) { isMouseLockEnabled_ = enabled; }

	// マウスの表示状態を設定
	void SetMouseVisible(bool visible) { isMouseVisible_ = visible; }

private:
    // コンストラクタとデストラクタ
    Input();
    ~Input();

    // コピーと代入を禁止
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

    // ゲームパッドの状態構造体
    struct GamepadState {
        XINPUT_STATE state;
        XINPUT_STATE prevState; // 前回の状態を保存
        bool isConnected;
        XINPUT_VIBRATION vibration;
    };

    // シングルトンのインスタンス
    static Input* instance_;

    // Windows API
    WinApp* winApp_;

    // DirectInput関連
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;

    // キーボードの状態
    BYTE key_[256] = {};
    BYTE keyPre_[256] = {};

	//マウスの状態
    float mouseDeltaX_ = 0.0f; // 前フレームからのマウスのX移動量
    float mouseDeltaY_ = 0.0f; // 前フレームからのマウスのY移動量
    int lastMouseX_ = 0;       // 前フレームのマウスX座標
    int lastMouseY_ = 0;       // 前フレームのマウスY座標
	POINT mousePos_;         // マウスの現在の座標
	// マウス固定
    bool isMouseLockEnabled_ = false;
    // マウスの表示
	bool isMouseVisible_ = true;
	bool preMouseVisible_ = true; // 前フレームのマウス表示状態

    // マウスボタンの状態
    BYTE mouseButtons_[3] = {};     // 現在のマウスボタンの状態（左:0, 中:1, 右:2）
    BYTE mouseButtonsPre_[3] = {};  // 前フレームのマウスボタンの状態

    // デッドゾーン
    float deadZone_;

    // ゲームパッドの状態
    std::array<GamepadState, XUSER_MAX_COUNT> gamepads_;

    // ボタンリマッピング
    struct InputBinding {
        InputType type;
        DWORD code;
    };
    std::unordered_map<Action, InputBinding> buttonMappings_;

    // アクションのコールバック
    std::unordered_map<Action, std::function<void()>> actionCallbacks_;

    // 入力の記録
    bool isRecording_;
    bool isPlaying_;
    std::vector<std::pair<Action, DWORD>> recordedInputs_;
    size_t playIndex_;
};
