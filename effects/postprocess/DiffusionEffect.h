#pragma once
#include "base/BasePostEffect.h"

namespace KCE
{
/** @brief 元画像をぼかしてスクリーン合成し、柔らかな滲みを作る。 */
class DiffusionEffect : public BasePostEffect
{
public:
	/** @brief 初期値で無効なディフュージョンを作る。 */
	DiffusionEffect();
	/** @brief 定数バッファへ現在値を反映する。 */
	void ApplyEffect(PostEffectParams& params) override;
	/** @brief 滲みの広さを設定する。 */
	void SetRadius(float radius);
	/** @brief 滲みの広さを返す。 */
	float GetRadius() const { return radius_; }
	/** @brief スクリーン合成量を設定する。 */
	void SetIntensity(float intensity);
	/** @brief スクリーン合成量を返す。 */
	float GetIntensity() const { return intensity_; }

private:
	float radius_ = 4.0f;
	float intensity_ = 0.35f;
};
} // namespace KCE
