#pragma once
#include "base/BasePostEffect.h"

namespace KCE
{
// ノイズエフェクトのデフォルトパラメータ
constexpr float kDefaultNoiseIntensity = 0.2f;      // デフォルトのノイズ強度
constexpr float kDefaultNoiseTime = 0.0f;           // デフォルトの時間
constexpr float kDefaultGrainSize = 1.0f;           // デフォルトの粒子サイズ
constexpr float kDefaultLuminanceAffect = 0.0f;     // デフォルトの輝度影響度

/**
 * @brief ノイズ（フィルムグレイン）エフェクトクラス
 *
 * フィルムカメラのグレイン（粒状ノイズ）を再現するポストエフェクト。
 * 時間パラメータによりノイズをアニメーションさせることで、
 * 動的なフィルム効果を表現できる。輝度影響度により、
 * 暗部にノイズを強調させることも可能。
 */
class NoiseEffect : public BasePostEffect
{
public:
    /**
     * @brief コンストラクタ
     *
     * デフォルトパラメータでノイズエフェクトを初期化する。
     * 初期状態ではエフェクトは無効。
     */
    NoiseEffect();

    /**
     * @brief デストラクタ
     */
    ~NoiseEffect() override;

    /**
     * @brief エフェクトを適用する
     * @param params ポストエフェクトパラメータ構造体への参照
     *
     * ノイズパラメータをparamsに設定する。
     */
    void ApplyEffect(PostEffectParams& params) override;

    /**
     * @brief ノイズの強度を設定する
     * @param intensity 強度 (0.0f～1.0f、大きいほどノイズが目立つ)
     */
    void SetIntensity(float intensity);

    /**
     * @brief ノイズアニメーション用の時間を設定する
     * @param time 時間パラメータ (毎フレーム更新することでノイズがアニメーション)
     */
    void SetTime(float time);

    /**
     * @brief ノイズ粒子のサイズを設定する
     * @param grainSize 粒子サイズ (1.0f: 標準サイズ)
     */
    void SetGrainSize(float grainSize);

    /**
     * @brief 輝度によるノイズ影響度を設定する
     * @param luminanceAffect 影響度 (0.0f: 全体に均一、1.0f: 暗部に強く影響)
     */
    void SetLuminanceAffect(float luminanceAffect);

    /**
     * @brief エフェクトの有効/無効を設定する
     * @param enabled true: エフェクト有効、false: エフェクト無効
     */
    void SetEnabled(bool enabled) override;

    /**
     * @brief ノイズの強度を取得する
     * @return 現在の強度 (0.0f～1.0f)
     */
    float GetIntensity() const { return params_.intensity; }

    /**
     * @brief 時間パラメータを取得する
     * @return 現在の時間
     */
    float GetTime() const { return params_.time; }

    /**
     * @brief ノイズ粒子のサイズを取得する
     * @return 現在の粒子サイズ
     */
    float GetGrainSize() const { return params_.grainSize; }

    /**
     * @brief 輝度によるノイズ影響度を取得する
     * @return 現在の影響度 (0.0f～1.0f)
     */
    float GetLuminanceAffect() const { return params_.luminanceAffect; }

private:
    /**
     * @brief ノイズエフェクトの内部パラメータ構造体
     */
    struct Parameters
    {
        // ノイズの強度 (0.0f～1.0f)
        float intensity;
        // ノイズアニメーション用時間
        float time;
        // ノイズ粒子のサイズ
        float grainSize;
        // 輝度によるノイズ影響度
        float luminanceAffect;
        // ノイズエフェクト有効フラグ
        int enabled;
        // 16バイトアラインメント用パディング
        float padding[3];
    };
    // 内部パラメータ
    Parameters params_;
};
} // namespace KCE
