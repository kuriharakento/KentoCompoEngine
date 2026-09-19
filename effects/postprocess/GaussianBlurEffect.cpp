#include "GaussianBlurEffect.h"

namespace KCE
{
GaussianBlurEffect::GaussianBlurEffect() = default;

void GaussianBlurEffect::ApplyEffect(PostEffectParams& params)
{
	params.gaussianBlurEnabled = enabled_ ? kEffectEnabled : kEffectDisabled;
	params.gaussianBlurRadius = radius_;
	params.gaussianBlurStrength = strength_;
}

void GaussianBlurEffect::SetRadius(float radius)
{
	radius_ = radius;
}

void GaussianBlurEffect::SetStrength(float strength)
{
	strength_ = strength;
}
} // namespace KCE
