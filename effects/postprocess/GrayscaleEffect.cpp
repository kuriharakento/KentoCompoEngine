#include "GrayscaleEffect.h"
#include <cassert>

namespace KCE
{
GrayscaleEffect::GrayscaleEffect()
{
    // デフォルトパラメータの設定
    params_.intensity = kDefaultGrayscaleIntensity;
	params_.enabled = kEffectDisabled;
	isDirty_ = true;
}

GrayscaleEffect::~GrayscaleEffect()
{
    
}

void GrayscaleEffect::ApplyEffect(PostEffectParams& params)
{
	if (enabled_)
	{
		// グレースケールエフェクトを有効にし、パラメータを設定
		params.grayscaleEnabled = kEffectEnabled;
		params.grayscaleIntensity = params_.intensity;
	}
	else
	{
		// グレースケールエフェクトを無効化
		params.grayscaleEnabled = kEffectDisabled;
	}
}

void GrayscaleEffect::SetIntensity(float intensity)
{
    // 値が変更された場合のみ更新フラグを立てる
    if (params_.intensity != intensity)
    {
        params_.intensity = intensity;
        isDirty_ = true;
    }
}

void GrayscaleEffect::SetEnabled(bool enabled)
{
	// 値が変更された場合のみ更新フラグを立てる
	if (enabled_ != enabled)
	{
		enabled_ = enabled;
		isDirty_ = true;
	}
}
} // namespace KCE
