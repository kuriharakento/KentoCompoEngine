#pragma once
#include "base/BasePostEffect.h"

// Bloomエフェクトのデフォルトパラメータ
constexpr float kDefaultBloomIntensity = 0.7f;    // デフォルトのBloom強度
constexpr float kDefaultBloomThreshold = 0.78f;   // デフォルトのしきい値
constexpr float kDefaultBloomRadius = 2.0f;       // デフォルトの半径
constexpr float kDefaultThresholdKnee = 0.5f;     // デフォルトのしきい値の緩やかさ
constexpr float kDefaultBloomMix = 1.0f;          // デフォルトの合成比率

/**
 * @brief Bloomエフェクトクラス
 *
 * 明るい部分を抽出してぼかし、元画像に加算合成することで
 * 光のにじみ（グロー効果）を表現するポストエフェクト。
 * しきい値以上の明るさを持つピクセルが発光して見える効果を生成する。
 */
class BloomEffect : public BasePostEffect
{
public:
    /**
     * @brief コンストラクタ
     *
     * デフォルトパラメータでBloomエフェクトを初期化する。
     * 初期状態ではエフェクトは有効。
     */
    BloomEffect();

    /**
     * @brief デストラクタ
     */
    ~BloomEffect() override;

    /**
     * @brief エフェクトを適用する
     * @param params ポストエフェクトパラメータ構造体への参照
     *
     * Bloomパラメータをparamsに設定する。
     */
    void ApplyEffect(PostEffectParams& params) override;

    /**
     * @brief Bloomの強度を設定する
     * @param intensity 強度 (0.0f～1.0f以上、大きいほど発光が強い)
     */
    void SetIntensity(float intensity);

    /**
     * @brief Bloomの強度を取得する
     * @return 現在の強度
     */
    float GetIntensity() const { return params_.intensity; }

    /**
     * @brief 明るさ抽出のしきい値を設定する
     * @param threshold しきい値 (0.0f～1.0f、この値以上の明るさがBloom対象)
     */
    void SetThreshold(float threshold);

    /**
     * @brief 明るさ抽出のしきい値を取得する
     * @return 現在のしきい値
     */
    float GetThreshold() const { return params_.threshold; }

    /**
     * @brief Bloomのぼかし半径を設定する
     * @param radius ぼかし半径 (大きいほど広範囲にぼける)
     */
    void SetRadius(float radius);

    /**
     * @brief Bloomのぼかし半径を取得する
     * @return 現在のぼかし半径
     */
    float GetRadius() const { return params_.radius; }

    /**
     * @brief エフェクトの有効/無効を設定する
     * @param enabled true: エフェクト有効、false: エフェクト無効
     */
    void SetEnabled(bool enabled) override;

    /**
     * @brief 画面サイズの逆数を取得する
     * @return 画面サイズの逆数 (UV計算用)
     */
	KCE::Vector2 GetInvScreenSize() const { return params_.invScreenSize; }

    /**
     * @brief 画面サイズの逆数を設定する
     * @param invScreenSize 画面サイズの逆数
     */
	void SetInvScreenSize(const KCE::Vector2& invScreenSize);

    /**
     * @brief しきい値の緩やかさを取得する
     * @return しきい値の緩やかさ (0.0f: 急峻、1.0f: 緩やか)
     */
	float GetThresholdKnee() const { return params_.thresholdKnee; }

    /**
     * @brief しきい値の緩やかさを設定する
     * @param thresholdKnee しきい値の緩やかさ (0.0f～1.0f)
     */
	void SetThresholdKnee(float thresholdKnee);

    /**
     * @brief Bloom合成比率を取得する
     * @return 合成比率 (1.0f: Bloomを完全に適用)
     */
	float GetBloomMix() const { return params_.bloomMix; }

    /**
     * @brief Bloom合成比率を設定する
     * @param bloomMix 合成比率 (0.0f～1.0f)
     */
	void SetBloomMix(float bloomMix);


private:
    /**
     * @brief Bloomエフェクトの内部パラメータ構造体
     */
    struct Parameters
    {
        // Bloomエフェクト有効フラグ
        int enabled;
        // Bloom強度
        float intensity;
        // 明るさ抽出のしきい値
        float threshold;
        // ぼかし半径
        float radius;
        // 16バイトアラインメント用パディング
        float padding[3];
        // 画面サイズの逆数
        KCE::Vector2 invScreenSize;
		// しきい値の緩やかさ
		float thresholdKnee;
		// オリジナル画像とBloom画像の合成比率
		float bloomMix;
    };
    // 内部パラメータ
    Parameters params_;
};
