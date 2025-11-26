#include "NoiseEffect.h"

NoiseEffect::NoiseEffect()
{
    // デフォルトパラメータの設定
    params_.intensity = kDefaultNoiseIntensity;
    params_.time = kDefaultNoiseTime;
    params_.grainSize = kDefaultGrainSize;
    params_.luminanceAffect = kDefaultLuminanceAffect;
    params_.enabled = kEffectDisabled;
	isDirty_ = true;
}

NoiseEffect::~NoiseEffect()
{
}

void NoiseEffect::ApplyEffect(PostEffectParams& params)
{
	if (enabled_)
	{
		// ノイズエフェクトを有効にし、パラメータを設定
		params.noiseEnabled = kEffectEnabled;
		params.noiseIntensity = params_.intensity;
		params.noiseTime = params_.time;
		params.grainSize = params_.grainSize;
		params.luminanceAffect = params_.luminanceAffect;
	}
	else
	{
		// ノイズエフェクトを無効化
		params.noiseEnabled = kEffectDisabled;
	}
}

void NoiseEffect::SetIntensity(float intensity)
{
    // 値が変更された場合のみ更新
    if (params_.intensity != intensity)
    {
        params_.intensity = intensity;
        isDirty_ = true;
    }
}

void NoiseEffect::SetTime(float time)
{
    // 値が変更された場合のみ更新
    if (params_.time != time)
    {
        params_.time = time;
        isDirty_ = true;
    }
}

void NoiseEffect::SetGrainSize(float grainSize)
{
    // 値が変更された場合のみ更新
    if (params_.grainSize != grainSize)
    {
        params_.grainSize = grainSize;
        isDirty_ = true;
    }
}

void NoiseEffect::SetLuminanceAffect(float luminanceAffect)
{
    // 値が変更された場合のみ更新
    if (params_.luminanceAffect != luminanceAffect)
    {
        params_.luminanceAffect = luminanceAffect;
        isDirty_ = true;
    }
}

void NoiseEffect::SetEnabled(bool enabled)
{
    // 値が変更された場合のみ更新
    if (enabled_ != enabled)
    {
        enabled_ = enabled;
        isDirty_ = true;
    }
}
