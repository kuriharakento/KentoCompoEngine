#include "BloomEffect.h"

#include "base/WinApp.h"

BloomEffect::BloomEffect()
{
    params_.intensity = 2.0f;
    params_.threshold = 0.78f;
    params_.radius = 2.0f;
    params_.enabled = 0;
    isDirty_ = true;
}

BloomEffect::~BloomEffect() {}

void BloomEffect::ApplyEffect(PostEffectParams& params)
{
    if (enabled_)
    {
        params.bloomEnabled = 1;
        params.bloomIntensity = params_.intensity;
        params.bloomThreshold = params_.threshold;
        params.bloomRadius = params_.radius;
    }
    else
    {
        params.bloomEnabled = 0;
    }
}

void BloomEffect::SetIntensity(float intensity)
{
    if (params_.intensity != intensity)
    {
        params_.intensity = intensity;
        isDirty_ = true;
    }
}

void BloomEffect::SetThreshold(float threshold)
{
    if (params_.threshold != threshold)
    {
        params_.threshold = threshold;
        isDirty_ = true;
    }
}

void BloomEffect::SetRadius(float radius)
{
    if (params_.radius != radius)
    {
        params_.radius = radius;
        isDirty_ = true;
    }
}

void BloomEffect::SetEnabled(bool enabled)
{
    if (enabled_ != enabled)
    {
        enabled_ = enabled;
        isDirty_ = true;
    }
}