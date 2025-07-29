#include "CRTEffect.h"

CRTEffect::CRTEffect()
{
	// デフォルトパラメータの設定
	params_.crtEnabled = 0; // CRTエフェクトは無効
	params_.scanlineEnabled = 0; // スキャンラインエフェクトは無効
	params_.scanlineIntensity = 0.5f; // スキャンラインの強度
	params_.scanlineCount = 10.0f; // スキャンラインの数
	params_.distortionEnabled = 0; // 歪みエフェクトは無効
	params_.distortionStrength = 0.1f; // 歪みの強度
	params_.chromAberrationEnabled = 0; // 色収差エフェクトは無効
	params_.chromAberrationOffset = 0.05f; // 色収差のオフセット
	isDirty_ = true; // 初期状態ではパラメータが変更されているとみなす
}

CRTEffect::~CRTEffect()
{
}

void CRTEffect::ApplyEffect(PostEffectParams& params)
{
	if (enabled_)
	{
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
		params.crtEnabled = 0; // CRTエフェクトを無効にする
	}
}

void CRTEffect::SetEnabled(bool enabled)
{
	if (enabled_ != enabled)
	{
		enabled_ = enabled;
		isDirty_ = true; // パラメータが変更されたことを示す
	}
}

void CRTEffect::SetCrtEnabled(int enabled)
{
	if (params_.crtEnabled != enabled)
	{
		params_.crtEnabled = enabled;
		isDirty_ = true; // パラメータが変更されたことを示す
	}
}

void CRTEffect::SetScanlineEnabled(int enabled)
{
	if (params_.scanlineEnabled != enabled)
	{
		params_.scanlineEnabled = enabled;
		isDirty_ = true; // パラメータが変更されたことを示す
	}
}

void CRTEffect::SetScanlineIntensity(float intensity)
{
	if (params_.scanlineIntensity != intensity)
	{
		params_.scanlineIntensity = intensity;
		isDirty_ = true; // パラメータが変更されたことを示す
	}
}

void CRTEffect::SetScanlineCount(float count)
{
	if (params_.scanlineCount != count)
	{
		params_.scanlineCount = count;
		isDirty_ = true; // パラメータが変更されたことを示す
	}
}

void CRTEffect::SetDistortionEnabled(int enabled)
{
	if (params_.distortionEnabled != enabled)
	{
		params_.distortionEnabled = enabled;
		isDirty_ = true; // パラメータが変更されたことを示す
	}
}

void CRTEffect::SetDistortionStrength(float strength)
{
	if (params_.distortionStrength != strength)
	{
		params_.distortionStrength = strength;
		isDirty_ = true; // パラメータが変更されたことを示す
	}
}

void CRTEffect::SetChromaticAberrationEnabled(int enabled)
{
	if (params_.chromAberrationEnabled != enabled)
	{
		params_.chromAberrationEnabled = enabled;
		isDirty_ = true; // パラメータが変更されたことを示す
	}
}

void CRTEffect::SetChromaticAberrationOffset(float offset)
{
	if (params_.chromAberrationOffset != offset)
	{
		params_.chromAberrationOffset = offset;
		isDirty_ = true; // パラメータが変更されたことを示す
	}
}
