#include "VignetteEffect.h"

VignetteEffect::VignetteEffect()
{
    // デフォルトパラメータの設定
    params_.intensity = kDefaultVignetteIntensity;
    params_.radius = kDefaultVignetteRadius;
    params_.softness = kDefaultVignetteSoftness;
    params_.color = { 0.0f, 0.0f, 0.0f };
    params_.enabled = kEffectDisabled;
	isDirty_ = true;
}

VignetteEffect::~VignetteEffect() {}

void VignetteEffect::ApplyEffect(PostEffectParams& params)
{
	if (enabled_)
	{
		// ビネットエフェクトを有効にし、パラメータを設定
		params.vignetteEnabled = kEffectEnabled;
		params.vignetteIntensity = params_.intensity;
		params.vignetteRadius = params_.radius;
		params.vignetteSoftness = params_.softness;
		params.vignetteColor = params_.color;
	}
	else
	{
		// ビネットエフェクトを無効化
		params.vignetteEnabled = kEffectDisabled;
	}
}

void VignetteEffect::SetIntensity(float intensity)
{
    // 値が変更された場合のみ更新
    if (params_.intensity != intensity)
    {
        params_.intensity = intensity;
        isDirty_ = true;
    }
}

void VignetteEffect::SetRadius(float radius)
{
    // 値が変更された場合のみ更新
    if (params_.radius != radius)
    {
        params_.radius = radius;
        isDirty_ = true;
    }
}

void VignetteEffect::SetSoftness(float softness)
{
    // 値が変更された場合のみ更新
    if (params_.softness != softness)
    {
        params_.softness = softness;
        isDirty_ = true;
    }
}

void VignetteEffect::SetColor(const Vector3& color)
{
    // 値が変更された場合のみ更新
    if (params_.color.x != color.x || params_.color.y != color.y || params_.color.z != color.z)
    {
        params_.color = color;
        isDirty_ = true;
    }
}

void VignetteEffect::SetEnabled(bool enabled)
{
    // 値が変更された場合のみ更新
    if (enabled_ != enabled)
    {
        enabled_ = enabled;
        isDirty_ = true;
    }
}
