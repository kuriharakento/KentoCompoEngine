#pragma once
#include "base/BasePostEffect.h"
#include "math/Vector3.h"

namespace KCE
{
/** @brief リフト・ガンマ・ゲインと彩度・コントラストを調整する。 */
class ColorGradingEffect : public BasePostEffect
{
public:
	/** @brief 初期値で無効なカラーグレーディングを作る。 */
	ColorGradingEffect();
	/** @brief 定数バッファへ現在値を反映する。 */
	void ApplyEffect(PostEffectParams& params) override;
	/** @brief 暗部へ加える色を設定する。 */
	void SetLift(const Vector3& lift);
	/** @brief 暗部へ加える色を返す。 */
	Vector3 GetLift() const { return lift_; }
	/** @brief 中間調のガンマを設定する。 */
	void SetGamma(const Vector3& gamma);
	/** @brief 中間調のガンマを返す。 */
	Vector3 GetGamma() const { return gamma_; }
	/** @brief 明部へ掛ける色を設定する。 */
	void SetGain(const Vector3& gain);
	/** @brief 明部へ掛ける色を返す。 */
	Vector3 GetGain() const { return gain_; }
	/** @brief 彩度を設定する。 */
	void SetSaturation(float saturation);
	/** @brief 彩度を返す。 */
	float GetSaturation() const { return saturation_; }
	/** @brief コントラストを設定する。 */
	void SetContrast(float contrast);
	/** @brief コントラストを返す。 */
	float GetContrast() const { return contrast_; }

private:
	Vector3 lift_ = { 0.0f, 0.0f, 0.0f };
	Vector3 gamma_ = { 1.0f, 1.0f, 1.0f };
	Vector3 gain_ = { 1.0f, 1.0f, 1.0f };
	float saturation_ = 1.0f;
	float contrast_ = 1.0f;
};
} // namespace KCE
