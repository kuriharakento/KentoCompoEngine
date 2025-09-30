#include "BloomEffect.h"

#include "base/WinApp.h"

BloomEffect::BloomEffect()
{
    params_.intensity = 0.7f;
    params_.threshold = 0.78f;
    params_.radius = 2.0f;
    params_.enabled = 1;
	params_.invScreenSize = { 1.0f / WinApp::kClientWidth, 1.0f / WinApp::kClientHeight };
	params_.thresholdKnee = 0.5f;
	params_.bloomMix = 1.0f;
	enabled_ = true;
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
		params.invScreenSize = params_.invScreenSize;
		params.bloomThresholdKnee = params_.thresholdKnee;
		params.bloomMix = params_.bloomMix;
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

void BloomEffect::SetInvScreenSize(const Vector2& invScreenSize)
{
	if (params_.invScreenSize.x != invScreenSize.x || params_.invScreenSize.y != invScreenSize.y)
	{
		params_.invScreenSize = invScreenSize;
		isDirty_ = true;
	}
}

void BloomEffect::SetThresholdKnee(float thresholdKnee)
{
	if (params_.thresholdKnee != thresholdKnee)
	{
		params_.thresholdKnee = thresholdKnee;
		isDirty_ = true;
	}
}

void BloomEffect::SetBloomMix(float bloomMix)
{
	if (params_.bloomMix != bloomMix)
	{
		params_.bloomMix = bloomMix;
		isDirty_ = true;
	}
}