#include "DiffusionEffect.h"

namespace KCE
{
DiffusionEffect::DiffusionEffect() = default;

void DiffusionEffect::ApplyEffect(PostEffectParams& params)
{
	params.diffusionEnabled = enabled_ ? kEffectEnabled : kEffectDisabled;
	params.diffusionRadius = radius_;
	params.diffusionIntensity = intensity_;
}

void DiffusionEffect::SetRadius(float radius)
{
	radius_ = radius;
}

void DiffusionEffect::SetIntensity(float intensity)
{
	intensity_ = intensity;
}
} // namespace KCE
