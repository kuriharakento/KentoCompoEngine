#include "BloomEffect.h"

#include "base/WinApp.h"

BloomEffect::BloomEffect()
{
    // デフォルトパラメータの設定
    params_.intensity = kDefaultBloomIntensity;
    params_.threshold = kDefaultBloomThreshold;
    params_.radius = kDefaultBloomRadius;
    params_.enabled = kEffectEnabled;
	params_.invScreenSize = { 1.0f / WinApp::kClientWidth, 1.0f / WinApp::kClientHeight };
	params_.thresholdKnee = kDefaultThresholdKnee;
	params_.bloomMix = kDefaultBloomMix;
	enabled_ = true;
    isDirty_ = true;
}

BloomEffect::~BloomEffect() {}

void BloomEffect::ApplyEffect(PostEffectParams& params)
{
    if (enabled_)
    {
        // Bloomエフェクトを有効にし、パラメータを設定
        params.bloomEnabled = kEffectEnabled;
        params.bloomIntensity = params_.intensity;
        params.bloomThreshold = params_.threshold;
        params.bloomRadius = params_.radius;
		params.invScreenSize = params_.invScreenSize;
		params.bloomThresholdKnee = params_.thresholdKnee;
		params.bloomMix = params_.bloomMix;
    }
    else
    {
        // Bloomエフェクトを無効化
        params.bloomEnabled = kEffectDisabled;
    }
}

void BloomEffect::SetIntensity(float intensity)
{
    // 値が変更された場合のみ更新
    if (params_.intensity != intensity)
    {
        params_.intensity = intensity;
        isDirty_ = true;
    }
}

void BloomEffect::SetThreshold(float threshold)
{
    // 値が変更された場合のみ更新
    if (params_.threshold != threshold)
    {
        params_.threshold = threshold;
        isDirty_ = true;
    }
}

void BloomEffect::SetRadius(float radius)
{
    // 値が変更された場合のみ更新
    if (params_.radius != radius)
    {
        params_.radius = radius;
        isDirty_ = true;
    }
}

void BloomEffect::SetEnabled(bool enabled)
{
    // 値が変更された場合のみ更新
    if (enabled_ != enabled)
    {
        enabled_ = enabled;
        isDirty_ = true;
    }
}

void BloomEffect::SetInvScreenSize(const Vector2& invScreenSize)
{
	// 値が変更された場合のみ更新
	if (params_.invScreenSize.x != invScreenSize.x || params_.invScreenSize.y != invScreenSize.y)
	{
		params_.invScreenSize = invScreenSize;
		isDirty_ = true;
	}
}

void BloomEffect::SetThresholdKnee(float thresholdKnee)
{
	// 値が変更された場合のみ更新
	if (params_.thresholdKnee != thresholdKnee)
	{
		params_.thresholdKnee = thresholdKnee;
		isDirty_ = true;
	}
}

void BloomEffect::SetBloomMix(float bloomMix)
{
	// 値が変更された場合のみ更新
	if (params_.bloomMix != bloomMix)
	{
		params_.bloomMix = bloomMix;
		isDirty_ = true;
	}
}