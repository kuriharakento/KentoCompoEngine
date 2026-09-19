#include "ColorGradingEffect.h"

namespace KCE
{
ColorGradingEffect::ColorGradingEffect() = default;

void ColorGradingEffect::ApplyEffect(PostEffectParams& params)
{
	params.colorGradingEnabled = enabled_ ? kEffectEnabled : kEffectDisabled;
	params.colorGradingLift = lift_;
	params.colorGradingGamma = gamma_;
	params.colorGradingGain = gain_;
	params.colorGradingSaturation = saturation_;
	params.colorGradingContrast = contrast_;
}

void ColorGradingEffect::SetLift(const Vector3& lift) { lift_ = lift; }
void ColorGradingEffect::SetGamma(const Vector3& gamma) { gamma_ = gamma; }
void ColorGradingEffect::SetGain(const Vector3& gain) { gain_ = gain; }
void ColorGradingEffect::SetSaturation(float saturation) { saturation_ = saturation; }
void ColorGradingEffect::SetContrast(float contrast) { contrast_ = contrast; }
} // namespace KCE
