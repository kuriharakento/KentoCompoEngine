#pragma once
#include "base/BasePostEffect.h"

namespace KCE
{
/** @brief トーンマップの既定の露出 */
constexpr float kDefaultTonemapExposure = 1.0f;

/**
 * @brief トーンマップの方式
 */
enum class TonemapMode
{
	ACES = 0,	  //!< ACESフィッティング。ハイライトの色転びが少なく、映画的な立ち上がり
	Reinhard = 1, //!< Reinhard。素直だがハイライトが眠くなりやすい
};

/**
 * @brief HDRの描画結果をLDRへ落とすトーンマップ
 *
 * @details SEQUENCER_PLAN 4.1。メインRTを R16G16B16A16_FLOAT にした結果、
 *          ライトパスの出力は 1.0 を超える値を保持するようになった。
 *          そのままでは表示できないため、ポストプロセスの最終段で
 *          この処理を通してLDRへ落とす。
 *
 *          ステージ照明の強い発光やビームは、この経路が無いと
 *          ブライトパスへ渡る前に白飽和して潰れてしまう。
 */
class TonemapEffect : public BasePostEffect
{
public:
	TonemapEffect();
	~TonemapEffect() override;

	void ApplyEffect(PostEffectParams& params) override;
	void SetEnabled(bool enabled) override;

	/**
	 * @brief 露出を設定する
	 * @details トーンマップ前に色へ乗算する。1.0が標準。
	 * @param exposure 露出
	 */
	void SetExposure(float exposure);

	float GetExposure() const { return params_.exposure; }

	/**
	 * @brief トーンマップの方式を設定する
	 * @param mode 方式
	 */
	void SetMode(TonemapMode mode);

	TonemapMode GetMode() const { return params_.mode; }

private:
	/**
	 * @brief トーンマップの内部パラメータ
	 */
	struct Parameters
	{
		float exposure;
		TonemapMode mode;
	};

	Parameters params_;
};
} // namespace KCE
