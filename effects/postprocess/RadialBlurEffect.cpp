#include "RadialBlurEffect.h"

namespace KCE
{
RadialBlurEffect::RadialBlurEffect() = default;

void RadialBlurEffect::ApplyEffect(PostEffectParams& params)
{
	params.radialBlurEnabled = enabled_ ? kEffectEnabled : kEffectDisabled;
	params.radialBlurCenter = center_;
	params.radialBlurStrength = strength_;
	params.radialBlurSampleCount = sampleCount_;
	params.radialBlurBlend = blend_;
}

void RadialBlurEffect::SetCenter(const Vector2& center)
{
	center_ = center;
}

void RadialBlurEffect::SetStrength(float strength)
{
	strength_ = strength;
}

void RadialBlurEffect::SetSampleCount(int sampleCount)
{
	sampleCount_ = sampleCount;
}

void RadialBlurEffect::SetBlend(float blend)
{
	blend_ = blend;
}
} // namespace KCE
