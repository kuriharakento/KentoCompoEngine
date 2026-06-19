#pragma once
#include "base/BasePostEffect.h"

// CRTエフェクトのデフォルトパラメータ
constexpr float kDefaultScanlineIntensity = 0.5f;       // デフォルトのスキャンライン強度
constexpr float kDefaultScanlineCount = 10.0f;          // デフォルトのスキャンライン数
constexpr float kDefaultDistortionStrength = 0.1f;      // デフォルトの歪み強度
constexpr float kDefaultChromAberrationOffset = 0.05f;  // デフォルトの色収差オフセット

/**
 * @brief CRT（ブラウン管モニター）エフェクトクラス
 *
 * 古いCRTモニターの視覚的特徴を再現するポストエフェクト。
 * スキャンライン（走査線）、樽型歪み、色収差の3つの効果を
 * 個別に有効/無効できる。レトロゲーム風の表現に使用される。
 */
class CRTEffect : public BasePostEffect
{
public:
	/**
	 * @brief コンストラクタ
	 *
	 * デフォルトパラメータでCRTエフェクトを初期化する。
	 * 初期状態ではエフェクトは無効。
	 */
	CRTEffect();

	/**
	 * @brief デストラクタ
	 */
	~CRTEffect() override;

	/**
	 * @brief エフェクトを適用する
	 * @param params ポストエフェクトパラメータ構造体への参照
	 *
	 * CRTパラメータをparamsに設定する。
	 */
	void ApplyEffect(PostEffectParams& params) override;

	/**
	 * @brief エフェクトの有効/無効を設定する
	 * @param enabled true: エフェクト有効、false: エフェクト無効
	 */
	void SetEnabled(bool enabled) override;

	/**
	 * @brief エフェクトが有効かどうかを取得する
	 * @return true: エフェクト有効、false: エフェクト無効
	 */
	bool IsEnabled() const override { return enabled_; }

	/**
	 * @brief CRTエフェクト全体の有効/無効を設定する
	 * @param enabled 1: 有効、0: 無効
	 */
	void SetCrtEnabled(int enabled);

	/**
	 * @brief CRTエフェクト全体が有効かどうかを取得する
	 * @return true: 有効、false: 無効
	 */
	bool IsCrtEnabled() const { return params_.crtEnabled; }

	/**
	 * @brief スキャンラインエフェクトの有効/無効を設定する
	 * @param enabled 1: 有効、0: 無効
	 */
	void SetScanlineEnabled(int enabled);

	/**
	 * @brief スキャンラインエフェクトが有効かどうかを取得する
	 * @return true: 有効、false: 無効
	 */
	bool IsScanlineEnabled() const { return params_.scanlineEnabled; }

	/**
	 * @brief スキャンラインの強度を設定する
	 * @param intensity 強度 (0.0f～1.0f、大きいほど走査線が目立つ)
	 */
	void SetScanlineIntensity(float intensity);

	/**
	 * @brief スキャンラインの強度を取得する
	 * @return 現在の強度 (0.0f～1.0f)
	 */
	float GetScanlineIntensity() const { return params_.scanlineIntensity; }

	/**
	 * @brief スキャンラインの本数を設定する
	 * @param count 本数 (画面の高さに対する走査線の数)
	 */
	void SetScanlineCount(float count);

	/**
	 * @brief スキャンラインの本数を取得する
	 * @return 現在の本数
	 */
	float GetScanlineCount() const { return params_.scanlineCount; }

	/**
	 * @brief 樽型歪みエフェクトの有効/無効を設定する
	 * @param enabled 1: 有効、0: 無効
	 */
	void SetDistortionEnabled(int enabled);

	/**
	 * @brief 樽型歪みエフェクトが有効かどうかを取得する
	 * @return true: 有効、false: 無効
	 */
	bool IsDistortionEnabled() const { return params_.distortionEnabled; }

	/**
	 * @brief 樽型歪みの強度を設定する
	 * @param strength 強度 (0.0f～1.0f、大きいほど歪みが強い)
	 */
	void SetDistortionStrength(float strength);

	/**
	 * @brief 樽型歪みの強度を取得する
	 * @return 現在の強度 (0.0f～1.0f)
	 */
	float GetDistortionStrength() const { return params_.distortionStrength; }

	/**
	 * @brief 色収差エフェクトの有効/無効を設定する
	 * @param enabled 1: 有効、0: 無効
	 */
	void SetChromaticAberrationEnabled(int enabled);

	/**
	 * @brief 色収差エフェクトが有効かどうかを取得する
	 * @return true: 有効、false: 無効
	 */
	bool IsChromaticAberrationEnabled() const { return params_.chromAberrationEnabled; }

	/**
	 * @brief 色収差のオフセットを設定する
	 * @param offset オフセット量 (RGBチャンネルのずれ量)
	 */
	void SetChromaticAberrationOffset(float offset);

	/**
	 * @brief 色収差のオフセットを取得する
	 * @return 現在のオフセット量
	 */
	float GetChromaticAberrationOffset() const { return params_.chromAberrationOffset; }

private:
	/**
	 * @brief CRTエフェクトの内部パラメータ構造体
	 */
	struct Parameters
	{
		// CRTエフェクト有効フラグ
		int crtEnabled;
		// スキャンラインエフェクト有効フラグ
		int scanlineEnabled;
		// スキャンラインの強度
		float scanlineIntensity;
		// スキャンラインの本数
		float scanlineCount;

		// 樽型歪みエフェクト有効フラグ
		int distortionEnabled;
		// 樽型歪みの強度
		float distortionStrength;
		// 色収差エフェクト有効フラグ
		int chromAberrationEnabled;
		// 色収差のオフセット量
		float chromAberrationOffset;

		// 16バイトアラインメント用パディング
		float pad3[4];
	};
	// 内部パラメータ
	Parameters params_;
};

