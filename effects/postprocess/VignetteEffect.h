#pragma once
#include "base/BasePostEffect.h"
#include "math/Vector3.h"

namespace KCE
{
// ビネットエフェクトのデフォルトパラメータ
constexpr float kDefaultVignetteIntensity = 1.0f;   // デフォルトの強度
constexpr float kDefaultVignetteRadius = 0.6f;      // デフォルトの半径
constexpr float kDefaultVignetteSoftness = 0.3f;    // デフォルトの柔らかさ

/**
 * @brief ビネットエフェクトクラス
 *
 * 画面の周辺部を暗くする（または着色する）ポストエフェクト。
 * カメラレンズの特性を再現し、画面中央に視線を誘導する効果がある。
 * 半径パラメータで明るい中心部分の広さを、柔らかさパラメータで
 * 暗くなるグラデーションの緩やかさを調整できる。
 */
class VignetteEffect : public BasePostEffect
{
public:
    /**
     * @brief コンストラクタ
     *
     * デフォルトパラメータでビネットエフェクトを初期化する。
     * 初期状態ではエフェクトは無効。
     */
    VignetteEffect();

    /**
     * @brief デストラクタ
     */
    ~VignetteEffect() override;

    /**
     * @brief エフェクトを適用する
     * @param params ポストエフェクトパラメータ構造体への参照
     *
     * ビネットパラメータをparamsに設定する。
     */
    void ApplyEffect(PostEffectParams& params) override;

    /**
     * @brief ビネットの強度を設定する
     * @param intensity 強度 (0.0f～1.0f、大きいほど暗くなる)
     */
    void SetIntensity(float intensity);

    /**
     * @brief ビネットの強度を取得する
     * @return 現在の強度 (0.0f～1.0f)
     */
    float GetIntensity() const { return params_.intensity; }

    /**
     * @brief ビネットの半径を設定する
     * @param radius 半径 (0.0f～1.0f、小さいほど中心部分が狭くなる)
     */
    void SetRadius(float radius);

    /**
     * @brief ビネットの半径を取得する
     * @return 現在の半径 (0.0f～1.0f)
     */
    float GetRadius() const { return params_.radius; }

    /**
     * @brief ビネットの柔らかさを設定する
     * @param softness 柔らかさ (0.0f～1.0f、大きいほど緩やかなグラデーション)
     */
    void SetSoftness(float softness);

    /**
     * @brief ビネットの柔らかさを取得する
     * @return 現在の柔らかさ (0.0f～1.0f)
     */
    float GetSoftness() const { return params_.softness; }

    /**
     * @brief ビネットの色を設定する
     * @param color 色 (RGB、通常は黒 {0,0,0})
     */
    void SetColor(const Vector3& color);

    /**
     * @brief ビネットの色を取得する
     * @return 現在の色 (RGB)
     */
    Vector3 GetColor() const { return params_.color; }

    /**
     * @brief エフェクトの有効/無効を設定する
     * @param enabled true: エフェクト有効、false: エフェクト無効
     */
    void SetEnabled(bool enabled) override;

private:
    /**
     * @brief ビネットエフェクトの内部パラメータ構造体
     */
    struct Parameters
    {
        // ビネットエフェクトが有効かどうか (0または1)
        int enabled;
        // ビネットの強度 (0.0f～1.0f)
        float intensity;
        // ビネットの半径 (0.0f～1.0f)
        float radius;
        // ビネットの柔らかさ (0.0f～1.0f)
        float softness;
        // ビネットの色 (RGB)
        Vector3 color;
        // 16バイトアラインメント用パディング
        float padding;
    };
    // 内部パラメータ
    Parameters params_;
};
} // namespace KCE
