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

/**
 * @brief ゲーム内で使用するアクションの列挙型
 * 
 * 入力デバイスに依存しないアクション定義。
 * ボタンリマッピング機能により、キーボードやゲームパッドの
 * 任意のボタンにアクションを割り当て可能。
 */
enum class Action {
    MoveLeft,    // 左移動
    MoveRight,   // 右移動
    Jump,        // ジャンプ
    Shoot,       // 射撃
    // 他のアクションを追加...
};

/**
 * @brief 入力デバイスの種類を表す列挙型
 */
enum class InputType {
    Keyboard, // キーボード（DirectInput経由）
    Gamepad   // ゲームパッド（XInput経由）
};

/**
 * @brief 入力管理クラス（シングルトン）
 * 
 * キーボード（DirectInput）、ゲームパッド（XInput）、マウスの入力を統合管理します。
 * 
 * 主な機能:
 * - キーボード入力: DirectInput8を使用した高精度なキー状態取得
 * - ゲームパッド入力: XInputを使用したXbox互換コントローラーのサポート
 * - マウス入力: Windows APIを使用した座標・ボタン状態取得
 * - デッドゾーン: アナログスティックのニュートラル位置での微小な入力を無視
 * - ボタンリマッピング: アクションベースの入力マッピング
 * - 入力記録/再生: デバッグやリプレイ用の入力記録機能
 * - マウス固定: FPSカメラ等で使用するマウス位置固定機能
 */
class Input {
public:
    /**
     * @brief シングルトンインスタンスを取得
     * @return Inputクラスのインスタンスへのポインタ
     */
    static Input* GetInstance();

    /**
     * @brief 入力システムを初期化
     * @param winApp ウィンドウアプリケーションへのポインタ
     */
    void Initialize(WinApp* winApp);

    /**
     * @brief 入力システムの終了処理
     */
    void Finalize();

    /**
     * @brief 入力状態を更新
     * 
     * 毎フレーム呼び出してキーボード、マウス、ゲームパッドの状態を更新します。
     */
    void Update();

    /**
     * @brief キーが押されているかチェック
     * @param keyNumber DirectInputのキーコード（DIK_*定数）
     * @return キーが押されている場合true
     */
    bool PushKey(BYTE keyNumber) const;

    /**
     * @brief キーが押された瞬間かチェック
     * @param keyNumber DirectInputのキーコード（DIK_*定数）
     * @return キーが押された瞬間の場合true
     */
    bool TriggerKey(BYTE keyNumber) const;

    /**
     * @brief キーが離された瞬間かチェック
     * @param keyNumber DirectInputのキーコード（DIK_*定数）
     * @return キーが離された瞬間の場合true
     */
    bool ReleaseTrigger(BYTE keyNumber) const;

    /**
     * @brief ゲームパッドのボタンが離された瞬間かチェック
     * @param gamepadIndex ゲームパッドのインデックス（0〜3）
     * @param buttonCode XInputのボタンコード（XINPUT_GAMEPAD_*定数）
     * @return ボタンが離された瞬間の場合true
     */
    bool ReleaseButton(DWORD gamepadIndex, DWORD buttonCode) const;

    /**
     * @brief デッドゾーンを設定
     * 
     * アナログスティックの入力がこの値以下の場合、入力なしとして扱います。
     * @param deadZone デッドゾーンの閾値（0.0〜1.0）
     */
    void SetDeadZone(float deadZone);

    /**
     * @brief ゲームパッドの振動を設定
     * @param gamepadIndex ゲームパッドのインデックス（0〜3）
     * @param leftMotor 左モーターの振動強度（0〜65535）
     * @param rightMotor 右モーターの振動強度（0〜65535）
     */
    void SetVibration(DWORD gamepadIndex, WORD leftMotor, WORD rightMotor);

    /**
     * @brief アクションに対する入力をリマッピング
     * @param action リマッピングするアクション
     * @param type 入力デバイスの種類
     * @param code 入力コード（キーコードまたはボタンコード）
     */
    void RemapButton(Action action, InputType type, DWORD code);

    /**
     * @brief アクションにコールバック関数をバインド
     * @param action バインドするアクション
     * @param callback アクション実行時に呼び出されるコールバック
     */
    void BindAction(Action action, std::function<void()> callback);

    /**
     * @brief 入力の記録を開始
     * 
     * アクションベースで入力を記録します。デバッグやリプレイ機能に使用。
     */
    void StartRecording();

    /**
     * @brief 入力の記録を停止
     */
    void StopRecording();

    /**
     * @brief 記録した入力を再生
     */
    void PlayRecording();

    /**
     * @brief ゲームパッドのボタンが押されているかチェック
     * @param gamepadIndex ゲームパッドのインデックス（0〜3）
     * @param buttonCode XInputのボタンコード（XINPUT_GAMEPAD_*定数）
     * @return ボタンが押されている場合true
     */
    bool IsButtonPressed(DWORD gamepadIndex, DWORD buttonCode) const;

    /**
     * @brief ゲームパッドのボタンが押された瞬間かチェック
     * @param gamepadIndex ゲームパッドのインデックス（0〜3）
     * @param buttonCode XInputのボタンコード（XINPUT_GAMEPAD_*定数）
     * @return ボタンが押された瞬間の場合true
     */
    bool IsButtonTriggered(DWORD gamepadIndex, DWORD buttonCode) const;

    /**
     * @brief マウスボタンが押されているかチェック
     * @param button ボタン番号（0:左、1:中、2:右）
     * @return ボタンが押されている場合true
     */
    bool IsMouseButtonPressed(int button) const;

    /**
     * @brief マウスボタンが押された瞬間かチェック
     * @param button ボタン番号（0:左、1:中、2:右）
     * @return ボタンが押された瞬間の場合true
     */
    bool IsMouseButtonTriggered(int button) const;

    /**
     * @brief マウスボタンが離された瞬間かチェック
     * @param button ボタン番号（0:左、1:中、2:右）
     * @return ボタンが離された瞬間の場合true
     */
    bool IsMouseButtonReleased(int button) const;

    /**
     * @brief マウスのX方向移動量を取得
     * @return X方向の移動量（ピクセル）
     */
    float GetMouseDeltaX() const;

    /**
     * @brief マウスのY方向移動量を取得
     * @return Y方向の移動量（ピクセル）
     */
    float GetMouseDeltaY() const;

    /**
     * @brief マウスのX座標を取得
     * @return クライアント領域内のX座標
     */
    float GetMouseX() const;

    /**
     * @brief マウスのY座標を取得
     * @return クライアント領域内のY座標
     */
    float GetMouseY() const;

    /**
     * @brief マウスの固定状態を設定
     * 
     * 有効にするとマウスカーソルがウィンドウ中央に固定され、
     * 移動量のみが取得可能になります。FPSカメラ等で使用。
     * @param enabled 固定を有効にする場合true
     */
    void SetMouseLockEnabled(bool enabled) { isMouseLockEnabled_ = enabled; }

    /**
     * @brief マウスカーソルの表示状態を設定
     * @param visible 表示する場合true
     */
    void SetMouseVisible(bool visible) { isMouseVisible_ = visible; }

private:
    Input();
    ~Input();

    // コピーと代入を禁止
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

    // ゲームパッドの状態構造体
    struct GamepadState {
        XINPUT_STATE state;      // 現在の状態
        XINPUT_STATE prevState;  // 前回の状態
        bool isConnected;        // 接続状態
        XINPUT_VIBRATION vibration; // 振動設定
    };

    static Input* instance_;                              // シングルトンインスタンス
    WinApp* winApp_;                                      // ウィンドウアプリケーション
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_;   // DirectInputインターフェース
    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_; // キーボードデバイス

    BYTE key_[256] = {};     // 現在のキーボード状態
    BYTE keyPre_[256] = {};  // 前フレームのキーボード状態

    float mouseDeltaX_ = 0.0f; // マウスのX移動量
    float mouseDeltaY_ = 0.0f; // マウスのY移動量
    int lastMouseX_ = 0;       // 前フレームのマウスX座標
    int lastMouseY_ = 0;       // 前フレームのマウスY座標
    POINT mousePos_;           // マウスの現在座標
    bool isMouseLockEnabled_ = false; // マウス固定フラグ
    bool isMouseVisible_ = true;      // マウス表示フラグ
    bool preMouseVisible_ = true;     // 前フレームのマウス表示状態

    BYTE mouseButtons_[3] = {};     // マウスボタン状態（左:0, 中:1, 右:2）
    BYTE mouseButtonsPre_[3] = {};  // 前フレームのマウスボタン状態

    float deadZone_; // アナログスティックのデッドゾーン閾値

    std::array<GamepadState, XUSER_MAX_COUNT> gamepads_; // ゲームパッド状態配列

    // ボタンリマッピング用構造体
    struct InputBinding {
        InputType type; // デバイス種類
        DWORD code;     // 入力コード
    };
    std::unordered_map<Action, InputBinding> buttonMappings_; // アクションマッピング

    std::unordered_map<Action, std::function<void()>> actionCallbacks_; // アクションコールバック

    // 入力記録/再生用
    bool isRecording_;                                 // 記録中フラグ
    bool isPlaying_;                                   // 再生中フラグ
    std::vector<std::pair<Action, DWORD>> recordedInputs_; // 記録された入力
    size_t playIndex_;                                 // 再生インデックス
};
