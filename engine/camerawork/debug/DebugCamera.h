#pragma once
// camerawork
#include "camerawork/base/CameraWorkBase.h"
// math
#include "math/Vector3.h"

/**
 * @brief デバッグ用フリーカメラクラス
 * 
 * WASD + マウスでシーン内を自由に移動・回転できるデバッグ用カメラ。
 * キーバインド：
 * - WASD: 前後左右移動
 * - Space: 上昇
 * - LShift: 下降
 * - マウス右クリック: 視点回転
 * - Tab: デバッグUIの表示切り替え
 * ImGuiデバッグUIで移動速度・マウス感度の調整が可能。
 */
class DebugCamera : public CameraWorkBase
{
public:
    DebugCamera() = default;
    virtual ~DebugCamera();

    /**
     * @brief カメラワークの初期化
     * @param camera 操作対象のカメラポインタ
     */
    void Initialize(Camera* camera) override;

    /**
     * @brief 毎フレームの更新処理
     * 
     * 移動・回転・速度制御・デバッグUI描画を行う。
     */
    void Update() override;

    /**
     * @brief デバッグカメラを開始
     * @param initialPosition 初期位置（デフォルト: {0.0f, 20.0f, -10.0f}）
     * @param initialRotation 初期回転（ラジアン、デフォルト: {1.1f, 0.0f, 0.0f}）
     */
    void Start(const Vector3& initialPosition = { 0.0f, 20.0f, -10.0f },
               const Vector3& initialRotation = { 1.1f, 0.0f, 0.0f });

    /**
     * @brief デバッグカメラを停止
     */
    void Stop();

    /**
     * @brief カメラを初期位置・向きにリセット
     */
    void Reset();

    /**
     * @brief 指定したターゲットにカメラを向ける
     * @param target フォーカスする対象の座標
     */
    void FocusOnTarget(const Vector3& target);

    /**
     * @brief 移動速度を設定
     * @param speed 移動速度（単位: 距離/秒）
     */
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }

    /**
     * @brief マウス感度を設定
     * @param sensitivity マウス感度係数
     */
    void SetMouseSensitivity(float sensitivity) { mouseSensitivity_ = sensitivity; }

    /**
     * @brief 速度倍率を設定
     * @param multiplier 速度倍率
     */
    void SetSpeedMultiplier(float multiplier) { speedMultiplier_ = multiplier; }

    /**
     * @brief アクティブ状態を取得
     * @return アクティブの場合true
     */
    bool IsActive() const { return isActive_; }

    /**
     * @brief 現在の移動速度を取得
     * @return 移動速度（単位: 距離/秒）
     */
    float GetMoveSpeed() const { return moveSpeed_; }

    /**
     * @brief 現在のマウス感度を取得
     * @return マウス感度係数
     */
    float GetMouseSensitivity() const { return mouseSensitivity_; }

private:
    /**
     * @brief 移動処理の更新
     */
    void UpdateMovement();

    /**
     * @brief マウスによる視点回転の更新
     */
    void UpdateMouseLook();

    /**
     * @brief ImGuiデバッグUIの描画
     */
    void DrawImGui();

    /**
     * @brief カメラの前方ベクトルを取得
     * @return 正規化された前方ベクトル
     */
    Vector3 GetForwardVector() const;

    /**
     * @brief カメラの右方ベクトルを取得
     * @return 正規化された右方ベクトル
     */
    Vector3 GetRightVector() const;

    /**
     * @brief カメラの上方ベクトルを取得
     * @return 正規化された上方ベクトル（常に{0, 1, 0}）
     */
    Vector3 GetUpVector() const;

private:
    // 基本移動速度（単位: 距離/秒）
    float moveSpeed_ = 5.0f;
    // 高速移動時の倍率（Shift押下時）
    float fastMoveMultiplier_ = 3.0f;
    // 低速移動時の倍率
    float slowMoveMultiplier_ = 0.3f;
    // 現在の速度倍率
    float speedMultiplier_ = 3.0f;

    // マウス感度係数
    float mouseSensitivity_ = 0.001f;

    // 水平回転角度（度）
    float yaw_ = 0.0f;
    // 垂直回転角度（度）
    float pitch_ = 0.0f;

    // アクティブ状態フラグ
    bool isActive_ = false;
    // デバッグUI表示フラグ
    bool showDebugUI_ = true;

    // Tabキーの前フレーム押下状態
    bool prevTabPressed_ = false;
};