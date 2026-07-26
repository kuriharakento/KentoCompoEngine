#pragma once
#include "base/BasePostEffect.h"

namespace KCE
{
// グレースケールエフェクトのデフォルト強度
constexpr float kDefaultGrayscaleIntensity = 1.0f;

/**
 * @brief グレースケールエフェクトクラス
 *
 * 画像をグレースケール（白黒）に変換するポストエフェクト。
 * 輝度計算により色情報を明るさ情報に変換し、
 * 強度パラメータで元画像との混合比率を調整できる。
 */
class GrayscaleEffect : public BasePostEffect
{
public:
    /**
     * @brief コンストラクタ
     *
     * デフォルトパラメータでグレースケールエフェクトを初期化する。
     * 初期状態ではエフェクトは無効。
     */
    GrayscaleEffect();

    /**
     * @brief デストラクタ
     */
    ~GrayscaleEffect() override;

    /**
     * @brief エフェクトを適用する
     * @param params ポストエフェクトパラメータ構造体への参照
     *
     * グレースケールパラメータをparamsに設定する。
     */
	void ApplyEffect(PostEffectParams& params) override;

    /**
     * @brief グレースケールの強度を設定する
     * @param intensity 強度 (0.0f: 元画像、1.0f: 完全なグレースケール)
     */
    void SetIntensity(float intensity);

    /**
     * @brief グレースケールの強度を取得する
     * @return 現在の強度 (0.0f～1.0f)
     */
    float GetIntensity() const { return params_.intensity; }

    /**
     * @brief エフェクトの有効/無効を設定する
     * @param enabled true: エフェクト有効、false: エフェクト無効
     */
    void SetEnabled(bool enabled) override;

private:
    /**
     * @brief グレースケールエフェクトの内部パラメータ構造体
     */
    struct Parameters
    {
        // グレースケールの強度 (0.0f～1.0f)
        float intensity;
        // エフェクトが有効かどうか (0または1)
        int enabled;
        // 16バイトアラインメントのためのパディング
        float padding[2];
    };
    // 内部パラメータ
    Parameters params_;
};
} // namespace KCE
