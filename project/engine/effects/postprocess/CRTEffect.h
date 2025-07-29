#pragma once
#include "base/BasePostEffect.h"

class CRTEffect : public BasePostEffect
{
public:
	CRTEffect();
	~CRTEffect() override;

	void ApplyEffect(PostEffectParams& params) override;

	void SetEnabled(bool enabled) override;
	bool IsEnabled() const override { return enabled_; }
	void SetCrtEnabled(int enabled);
	bool IsCrtEnabled() const { return params_.crtEnabled; }
	void SetScanlineEnabled(int enabled);
	bool IsScanlineEnabled() const { return params_.scanlineEnabled; }
	void SetScanlineIntensity(float intensity);
	float GetScanlineIntensity() const { return params_.scanlineIntensity; }
	void SetScanlineCount(float count);
	float GetScanlineCount() const { return params_.scanlineCount; }
	void SetDistortionEnabled(int enabled);
	bool IsDistortionEnabled() const { return params_.distortionEnabled; }
	void SetDistortionStrength(float strength);
	float GetDistortionStrength() const { return params_.distortionStrength; }
	void SetChromaticAberrationEnabled(int enabled);
	bool IsChromaticAberrationEnabled() const { return params_.chromAberrationEnabled; }
	void SetChromaticAberrationOffset(float offset);
	float GetChromaticAberrationOffset() const { return params_.chromAberrationOffset; }

private:
	struct Parameters
	{
		int crtEnabled;
		int scanlineEnabled;
		float scanlineIntensity;
		float scanlineCount;

		int distortionEnabled;
		float distortionStrength;
		int chromAberrationEnabled;
		float chromAberrationOffset;

		float pad3[4];
	};
	Parameters params_;
};

