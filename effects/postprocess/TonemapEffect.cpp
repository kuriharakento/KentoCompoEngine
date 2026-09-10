#include "TonemapEffect.h"

namespace KCE
{
TonemapEffect::TonemapEffect()
{
	params_.exposure = kDefaultTonemapExposure;
	params_.mode = TonemapMode::ACES;

	// メインRTがHDRである以上、既定で有効でなければ絵が破綻する
	enabled_ = true;
	isDirty_ = true;
}

TonemapEffect::~TonemapEffect()
{
}

void TonemapEffect::ApplyEffect(PostEffectParams& params)
{
	if (enabled_)
	{
		params.tonemapEnabled = kEffectEnabled;
		params.tonemapExposure = params_.exposure;
		params.tonemapMode = static_cast<int>(params_.mode);
	}
	else
	{
		// 無効にするとHDR値がそのまま出るため、確認用途以外では使わないこと
		params.tonemapEnabled = kEffectDisabled;
	}
}

void TonemapEffect::SetEnabled(bool enabled)
{
	if (enabled_ != enabled)
	{
		enabled_ = enabled;
		isDirty_ = true;
	}
}

void TonemapEffect::SetExposure(float exposure)
{
	if (params_.exposure != exposure)
	{
		params_.exposure = exposure;
		isDirty_ = true;
	}
}

void TonemapEffect::SetMode(TonemapMode mode)
{
	if (params_.mode != mode)
	{
		params_.mode = mode;
		isDirty_ = true;
	}
}
} // namespace KCE
