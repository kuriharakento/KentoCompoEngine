#include "NoiseEffect.h"

NoiseEffect::NoiseEffect()
{
    params_.intensity = 0.2f;
    params_.time = 0.0f;
    params_.grainSize = 1.0f;
    params_.luminanceAffect = 0.0f;
    params_.enabled = 0;
	isDirty_ = true; // 初期状態ではパラメータが変更されているとみなす
}

NoiseEffect::~NoiseEffect()
{
}

void NoiseEffect::ApplyEffect(PostEffectParams& params)
{
	if (enabled_)
	{
		params.noiseEnabled = 1; // ノイズエフェクトを有効にする
		params.noiseIntensity = params_.intensity;
		params.noiseTime = params_.time;
		params.grainSize = params_.grainSize;
		params.luminanceAffect = params_.luminanceAffect;
	}
	else
	{
		params.noiseEnabled = 0; // ノイズエフェクトを無効にする
	}
}

void NoiseEffect::SetIntensity(float intensity)
{
    if (params_.intensity != intensity)
    {
        params_.intensity = intensity;
        isDirty_ = true;
    }
}

void NoiseEffect::SetTime(float time)
{
    if (params_.time != time)
    {
        params_.time = time;
        isDirty_ = true;
    }
}

void NoiseEffect::SetGrainSize(float grainSize)
{
    if (params_.grainSize != grainSize)
    {
        params_.grainSize = grainSize;
        isDirty_ = true;
    }
}

void NoiseEffect::SetLuminanceAffect(float luminanceAffect)
{
    if (params_.luminanceAffect != luminanceAffect)
    {
        params_.luminanceAffect = luminanceAffect;
        isDirty_ = true;
    }
}

void NoiseEffect::SetEnabled(bool enabled)
{
    if (enabled_ != enabled)
    {
        enabled_ = enabled;
        isDirty_ = true;
    }
}
