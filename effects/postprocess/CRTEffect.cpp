#include "CRTEffect.h"

namespace KCE
{
CRTEffect::CRTEffect()
{
	// デフォルトパラメータの設定
	params_.crtEnabled = kEffectDisabled;
	params_.scanlineEnabled = kEffectDisabled;
	params_.scanlineIntensity = kDefaultScanlineIntensity;
	params_.scanlineCount = kDefaultScanlineCount;
	params_.distortionEnabled = kEffectDisabled;
	params_.distortionStrength = kDefaultDistortionStrength;
	params_.chromAberrationEnabled = kEffectDisabled;
	params_.chromAberrationOffset = kDefaultChromAberrationOffset;
	isDirty_ = true;
}

CRTEffect::~CRTEffect()
{
}

void CRTEffect::ApplyEffect(PostEffectParams& params)
{
	if (enabled_)
	{
		// CRTエフェクトのパラメータを設定
		params.crtEnabled = params_.crtEnabled;
		params.scanlineEnabled = params_.scanlineEnabled;
		params.scanlineIntensity = params_.scanlineIntensity;
		params.scanlineCount = params_.scanlineCount;
		params.distortionEnabled = params_.distortionEnabled;
		params.distortionStrength = params_.distortionStrength;
		params.chromAberrationEnabled = params_.chromAberrationEnabled;
		params.chromAberrationOffset = params_.chromAberrationOffset;
	}
	else
	{
		// CRTエフェクトを無効化
		params.crtEnabled = kEffectDisabled;
	}
}

void CRTEffect::SetEnabled(bool enabled)
{
	// 値が変更された場合のみ更新
	if (enabled_ != enabled)
	{
		enabled_ = enabled;
		isDirty_ = true;
	}
}

void CRTEffect::SetCrtEnabled(int enabled)
{
	// 値が変更された場合のみ更新
	if (params_.crtEnabled != enabled)
	{
		params_.crtEnabled = enabled;
		isDirty_ = true;
	}
}

void CRTEffect::SetScanlineEnabled(int enabled)
{
	// 値が変更された場合のみ更新
	if (params_.scanlineEnabled != enabled)
	{
		params_.scanlineEnabled = enabled;
		isDirty_ = true;
	}
}

void CRTEffect::SetScanlineIntensity(float intensity)
{
	// 値が変更された場合のみ更新
	if (params_.scanlineIntensity != intensity)
	{
		params_.scanlineIntensity = intensity;
		isDirty_ = true;
	}
}

void CRTEffect::SetScanlineCount(float count)
{
	// 値が変更された場合のみ更新
	if (params_.scanlineCount != count)
	{
		params_.scanlineCount = count;
		isDirty_ = true;
	}
}

void CRTEffect::SetDistortionEnabled(int enabled)
{
	// 値が変更された場合のみ更新
	if (params_.distortionEnabled != enabled)
	{
		params_.distortionEnabled = enabled;
		isDirty_ = true;
	}
}

void CRTEffect::SetDistortionStrength(float strength)
{
	// 値が変更された場合のみ更新
	if (params_.distortionStrength != strength)
	{
		params_.distortionStrength = strength;
		isDirty_ = true;
	}
}

void CRTEffect::SetChromaticAberrationEnabled(int enabled)
{
	// 値が変更された場合のみ更新
	if (params_.chromAberrationEnabled != enabled)
	{
		params_.chromAberrationEnabled = enabled;
		isDirty_ = true;
	}
}

void CRTEffect::SetChromaticAberrationOffset(float offset)
{
	// 値が変更された場合のみ更新
	if (params_.chromAberrationOffset != offset)
	{
		params_.chromAberrationOffset = offset;
		isDirty_ = true;
	}
}
} // namespace KCE
