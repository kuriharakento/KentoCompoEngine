#pragma once
#include <cstddef>
#include "effects/postprocess/IPostEffect.h"
#include "math/Vector2.h"
#include "math/Vector3.h"

namespace KCE
{
// GPU定数バッファのアラインメントサイズ（16バイト境界）
constexpr size_t kAlignmentSize = 16;

// エフェクトの有効/無効フラグ
constexpr int kEffectDisabled = 0;
constexpr int kEffectEnabled = 1;

/**
 * @brief ポストエフェクト用パラメータ構造体
 *
 * GPU定数バッファとして使用するため、16バイトアラインメントが必要。
 * 各エフェクトのパラメータを一括で管理し、シェーダーに渡す。
 */
struct alignas(16) PostEffectParams
{
	// --- グレースケール ---
	// グレースケールの強度 (0.0f: 元画像、1.0f: 完全なグレースケール)
	float grayscaleIntensity;
	// グレースケールエフェクト有効フラグ (0: 無効、1: 有効)
	int grayscaleEnabled;
	// 16バイトアラインメント用パディング
	float pad0[2];

	// --- ヴィネット ---
	// ビネットエフェクト有効フラグ (0: 無効、1: 有効)
	int vignetteEnabled;
	// ビネットの強度 (0.0f～1.0f)
	float vignetteIntensity;
	// ビネットの半径 (0.0f～1.0f、小さいほど中心部分が狭くなる)
	float vignetteRadius;
	// ビネットのエッジの柔らかさ (0.0f～1.0f、大きいほど緩やかなグラデーション)
	float vignetteSoftness;

	// ビネットの色 (RGB、通常は黒 {0,0,0})
	Vector3 vignetteColor;
	// 16バイトアラインメント用パディング
	float pad1;

	// --- ノイズ ---
	// ノイズエフェクト有効フラグ (0: 無効、1: 有効)
	int noiseEnabled;
	// ノイズの強度 (0.0f～1.0f)
	float noiseIntensity;
	// ノイズのアニメーション用時間パラメータ
	float noiseTime;
	// ノイズ粒子のサイズ (1.0f: 標準サイズ)
	float grainSize;

	// 輝度によるノイズ影響度 (0.0f: 全体に均一、1.0f: 暗部に強く影響)
	float luminanceAffect;
	// 16バイトアラインメント用パディング
	float pad2[3];

	/** @brief CRT */
	// CRTエフェクト有効フラグ (0: 無効、1: 有効)
	int crtEnabled;
	// スキャンラインエフェクト有効フラグ (0: 無効、1: 有効)
	int scanlineEnabled;
	// スキャンラインの強度 (0.0f～1.0f)
	float scanlineIntensity;
	// スキャンラインの本数
	float scanlineCount;

	// 樽型歪みエフェクト有効フラグ (0: 無効、1: 有効)
	int distortionEnabled;
	// 樽型歪みの強度 (0.0f～1.0f)
	float distortionStrength;
	// 色収差エフェクト有効フラグ (0: 無効、1: 有効)
	int chromAberrationEnabled;
	// 色収差のオフセット量 (ピクセル単位)
	float chromAberrationOffset;

	// 16バイトアラインメント用パディング
	float pad3[4];

	/** @brief Bloom */
	// Bloomエフェクト有効フラグ (0: 無効、1: 有効)
	int bloomEnabled;
	// Bloomの強度 (0.0f～1.0f以上)
	float bloomIntensity;
	// 明るさ抽出のしきい値 (この値以上の明るさがBloom対象)
	float bloomThreshold;
	// Bloomのぼかし半径
	float bloomRadius;
	// 16バイトアラインメント用パディング
	//
	// HLSL側は float3 pad4 → float2 invScreenSize と並んでいるが、
	// HLSLの定数バッファでは変数が16バイト境界をまたぐことを許さないため、
	// invScreenSize は pad4 の直後（156）ではなく次の境界（160）へ送られる。
	// C++側は詰めて配置されるので、ここで4バイト分を明示的に埋めないと
	// これ以降の全フィールドが4バイトずれて読まれる。
	float pad4[4];

	// 画面サイズの逆数 (シェーダー内でのUV計算用)
	Vector2 invScreenSize;
	// しきい値の緩やかさ (0.0f: 急峻、1.0f: 緩やか)
	float bloomThresholdKnee;
	// オリジナル画像とBloom画像の合成比率 (1.0f: Bloomを完全に適用)
	float bloomMix;

	/** @brief トーンマップ */
	// トーンマップ有効フラグ (0: 無効、1: 有効)
	// メインRTがHDRになったため、最終段でLDRへ落とすこの処理が前提になる
	int tonemapEnabled;
	// 露出。トーンマップ前に色へ乗算する
	float tonemapExposure;
	// トーンマップの種類 (0: ACES、1: Reinhard)
	int tonemapMode;
	// 16バイトアラインメント用パディング
	float pad5;

	/** @brief ガウシアンブラー */
	int gaussianBlurEnabled;
	float gaussianBlurRadius;
	float gaussianBlurStrength;
	float pad6;

	/** @brief ディフュージョン */
	int diffusionEnabled;
	float diffusionRadius;
	float diffusionIntensity;
	float pad7;

	/** @brief ラジアルブラー */
	int radialBlurEnabled;
	int radialBlurSampleCount;
	Vector2 radialBlurCenter;
	float radialBlurStrength;
	float radialBlurBlend;
	float pad8[2];

	/** @brief カラーグレーディング */
	int colorGradingEnabled;
	float pad9[3];
	Vector3 colorGradingLift;
	float pad10;
	Vector3 colorGradingGamma;
	float pad11;
	Vector3 colorGradingGain;
	float pad12;
	float colorGradingSaturation;
	float colorGradingContrast;
	float pad13[2];

	/**
	 * @brief パラメータの等価比較演算子
	 * @param other 比較対象のパラメータ
	 * @return true: 全てのパラメータが一致、false: 一つ以上のパラメータが異なる
	 */
	bool operator==(const PostEffectParams& other) const
	{
		return
			grayscaleIntensity == other.grayscaleIntensity &&
			grayscaleEnabled == other.grayscaleEnabled &&
			vignetteEnabled == other.vignetteEnabled &&
			vignetteIntensity == other.vignetteIntensity &&
			vignetteRadius == other.vignetteRadius &&
			vignetteSoftness == other.vignetteSoftness &&
			vignetteColor == other.vignetteColor &&
			noiseEnabled == other.noiseEnabled &&
			noiseIntensity == other.noiseIntensity &&
			noiseTime == other.noiseTime &&
			grainSize == other.grainSize &&
			luminanceAffect == other.luminanceAffect &&
			crtEnabled == other.crtEnabled &&
			scanlineEnabled == other.scanlineEnabled &&
			scanlineIntensity == other.scanlineIntensity &&
			scanlineCount == other.scanlineCount &&
			distortionEnabled == other.distortionEnabled &&
			distortionStrength == other.distortionStrength &&
			chromAberrationEnabled == other.chromAberrationEnabled &&
			chromAberrationOffset == other.chromAberrationOffset &&
			bloomEnabled == other.bloomEnabled &&
			bloomIntensity == other.bloomIntensity &&
			bloomThreshold == other.bloomThreshold &&
			bloomRadius == other.bloomRadius &&
			invScreenSize == other.invScreenSize &&
			bloomThresholdKnee == other.bloomThresholdKnee &&
			bloomMix == other.bloomMix &&
			tonemapEnabled == other.tonemapEnabled &&
			tonemapExposure == other.tonemapExposure &&
			tonemapMode == other.tonemapMode &&
			gaussianBlurEnabled == other.gaussianBlurEnabled &&
			gaussianBlurRadius == other.gaussianBlurRadius &&
			gaussianBlurStrength == other.gaussianBlurStrength &&
			diffusionEnabled == other.diffusionEnabled &&
			diffusionRadius == other.diffusionRadius &&
			diffusionIntensity == other.diffusionIntensity &&
			radialBlurEnabled == other.radialBlurEnabled &&
			radialBlurSampleCount == other.radialBlurSampleCount &&
			radialBlurCenter == other.radialBlurCenter &&
			radialBlurStrength == other.radialBlurStrength &&
			radialBlurBlend == other.radialBlurBlend &&
			colorGradingEnabled == other.colorGradingEnabled &&
			colorGradingLift == other.colorGradingLift &&
			colorGradingGamma == other.colorGradingGamma &&
			colorGradingGain == other.colorGradingGain &&
			colorGradingSaturation == other.colorGradingSaturation &&
			colorGradingContrast == other.colorGradingContrast;
	}

	/**
	 * @brief パラメータの非等価比較演算子
	 * @param other 比較対象のパラメータ
	 * @return true: 一つ以上のパラメータが異なる、false: 全てのパラメータが一致
	 */
	bool operator!=(const PostEffectParams& other) const
	{
		return !(*this == other);
	}
};

// HLSL側の cbuffer PostEffectParams（PostEffect.PS.hlsl）と
// バイト単位で一致していることを検証する。
//
// ここがずれると、シェーダーは隣のフィールドの値を読むことになる。
// コンパイルも実行も通ってしまい、絵が真っ暗になる・エフェクトが効かない
// といった形でしか現れないため、必ず静的に検証しておくこと。
// フィールドを足すときは、この一覧にも追記する。
static_assert(offsetof(PostEffectParams, grayscaleIntensity) == 0, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, vignetteEnabled) == 16, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, vignetteColor) == 32, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, noiseEnabled) == 48, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, luminanceAffect) == 64, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, crtEnabled) == 80, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, distortionEnabled) == 96, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, bloomEnabled) == 128, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, invScreenSize) == 160, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, bloomThresholdKnee) == 168, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, bloomMix) == 172, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, tonemapEnabled) == 176, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, tonemapExposure) == 180, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, tonemapMode) == 184, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, gaussianBlurEnabled) == 192, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, diffusionEnabled) == 208, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, radialBlurEnabled) == 224, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, radialBlurStrength) == 240, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, radialBlurBlend) == 244, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, colorGradingEnabled) == 256, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, colorGradingLift) == 272, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, colorGradingGamma) == 288, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, colorGradingGain) == 304, "HLSLのレイアウトと一致していません");
static_assert(offsetof(PostEffectParams, colorGradingSaturation) == 320, "HLSLのレイアウトと一致していません");
static_assert(sizeof(PostEffectParams) == 336, "HLSLのレイアウトと一致していません");

/**
 * @brief ポストエフェクトの基底クラス
 *
 * すべてのポストエフェクトの共通機能を提供する抽象基底クラス。
 * エフェクトの有効/無効管理と、パラメータ変更検知機能を持つ。
 */
class BasePostEffect : public IPostEffect
{
public:
	/**
	 * @brief コンストラクタ
	 */
	BasePostEffect();

	/**
	 * @brief デストラクタ
	 */
	virtual ~BasePostEffect();

	/**
	 * @brief エフェクトを適用する（純粋仮想関数）
	 * @param params ポストエフェクトパラメータ構造体への参照
	 *
	 * 派生クラスで実装し、自身のパラメータをparamsに設定する。
	 */
	virtual void ApplyEffect(PostEffectParams& params) = 0;

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

protected:
	// エフェクトが有効かどうか
	bool enabled_ = false;
	// パラメータが変更されたかのフラグ（定数バッファ更新判定用）
	bool isDirty_ = true;

};
} // namespace KCE
