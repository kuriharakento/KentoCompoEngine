#pragma once
#include "base/BasePostEffect.h"

namespace KCE
{
/** @brief 画面全体へガウシアンブラーを掛ける。 */
class GaussianBlurEffect : public BasePostEffect
{
public:
	/** @brief 初期値で無効なブラーを作る。 */
	GaussianBlurEffect();
	/** @brief 定数バッファへ現在値を反映する。 */
	void ApplyEffect(PostEffectParams& params) override;
	/** @brief サンプル間隔を設定する。 */
	void SetRadius(float radius);
	/** @brief サンプル間隔を返す。 */
	float GetRadius() const { return radius_; }
	/** @brief 元画像との合成量を設定する。 */
	void SetStrength(float strength);
	/** @brief 元画像との合成量を返す。 */
	float GetStrength() const { return strength_; }

private:
	float radius_ = 2.0f;
	float strength_ = 1.0f;
};
} // namespace KCE
