#include "VignetteEffect.h"

VignetteEffect::VignetteEffect()
{
    // デフォルトパラメータ
    params_.intensity = 1.0f;
    params_.radius = 0.6f;
    params_.softness = 0.3f;
    params_.color = { 0.0f, 0.0f, 0.0f };
    params_.enabled = 0;
	isDirty_ = true; // 初期状態ではパラメータが変更されているとみなす
}

VignetteEffect::~VignetteEffect() {}

void VignetteEffect::ApplyEffect(PostEffectParams& params)
{
	if (enabled_)
	{
		params.vignetteEnabled = 1; // ビネットエフェクトを有効にする
		params.vignetteIntensity = params_.intensity;
		params.vignetteRadius = params_.radius;
		params.vignetteSoftness = params_.softness;
		params.vignetteColor = params_.color;
	}
	else
	{
		params.vignetteEnabled = 0; // ビネットエフェクトを無効にする
	}
}

void VignetteEffect::SetIntensity(float intensity)
{
    if (params_.intensity != intensity)
    {
        params_.intensity = intensity;
        isDirty_ = true;
    }
}

void VignetteEffect::SetRadius(float radius)
{
    if (params_.radius != radius)
    {
        params_.radius = radius;
        isDirty_ = true;
    }
}

void VignetteEffect::SetSoftness(float softness)
{
    if (params_.softness != softness)
    {
        params_.softness = softness;
        isDirty_ = true;
    }
}

void VignetteEffect::SetColor(const Vector3& color)
{
    if (params_.color.x != color.x || params_.color.y != color.y || params_.color.z != color.z)
    {
        params_.color = color;
        isDirty_ = true;
    }
}

void VignetteEffect::SetEnabled(bool enabled)
{
    if (enabled_ != enabled)
    {
        enabled_ = enabled;
        isDirty_ = true;
    }
}
