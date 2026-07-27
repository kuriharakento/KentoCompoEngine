#pragma once
// camerawork
#include "camerawork/base/CameraWorkBase.h"

// math
#include "math/Vector3.h"
#include <numbers>

namespace KCE
{
class Camera;

/**
 * @brief 見下ろしカメラクラス
 * 
 * 指定したターゲットを上方から見下ろすカメラワーク。
 * ターゲットの移動に追従し、滑らかな補間で移動する。
 * 高さ・ピッチ・ヨーの調整が可能。
 */
class TopDownCamera : public CameraWorkBase
{
public:
    /**
     * @brief カメラワークの初期化
     * @param camera 操作対象のカメラポインタ
     */
    void Initialize(Camera* camera)override;
    virtual ~TopDownCamera();

#ifdef USE_IMGUI
    void DrawImGui();
#endif

    /**
     * @brief 毎フレームの更新処理
     * 
     * ターゲットを追従し、カメラ位置・回転を更新する。
     * ImGuiデバッグUIも描画する。
     */
    void Update() override;

    /**
     * @brief 見下ろしカメラを開始
     * @param height カメラの高さ（ターゲットからの距離）
     * @param target 追従するターゲットの座標ポインタ
     */
    void Start(float height, const Vector3* target);

    /**
     * @brief 追従ターゲットを設定
     * @param target 追従するターゲットの座標ポインタ
     */
    void SetTarget(const Vector3* target);

    /**
     * @brief カメラの高さを設定
     * @param height カメラの高さ（ターゲットからの距離）
     */
    void SetHeight(float height);

    /**
     * @brief 現在の高さを取得
     * @return カメラの高さ
     */
    float GetHeight() const { return height_; }

    /**
     * @brief アクティブ状態を設定
     * @param active アクティブ状態
     */
    void SetActive(bool active);

    /**
     * @brief ピッチ角を設定
     * @param pitch ピッチ角（ラジアン）
     */
    void SetPitch(float pitch) { pitch_ = pitch; }

    /**
     * @brief 現在のピッチ角を取得
     * @return ピッチ角（ラジアン）
     */
    float GetPitch() const { return pitch_; }

    /**
     * @brief ヨー角を設定
     * @param yaw ヨー角（ラジアン）
     */
    void SetYaw(float yaw) { yaw_ = yaw; }

    /**
     * @brief 現在のヨー角を取得
     * @return ヨー角（ラジアン）
     */
    float GetYaw() const { return yaw_; }

    /**
     * @brief カメラ位置のオフセットを設定
     * @param offset オフセット座標
     */
    void SetOffset(const Vector3& offset) { offset_ = offset; }

    /**
     * @brief 現在のオフセットを取得
     * @return オフセット座標
     */
    Vector3 GetOffset() const { return offset_; }
   

private:
    // 操作対象のカメラ
    Camera* camera_ = nullptr;
    // 追従するターゲットのポインタ
    const Vector3* target_ = nullptr;
    // カメラの高さ（ターゲットからの距離）
    float height_ = 10.0f;
    // X軸角度（ラジアン、デフォルトで真下を向く）
    float pitch_ = std::numbers::pi_v<float> / 2.0f;
    // Y軸角度（ラジアン）
    float yaw_ = 0.0f;
    // カメラの位置オフセット
    Vector3 offset_ = {};
    // アクティブ状態フラグ
    bool isActive_ = false;
};
} // namespace KCE
