#include "GrayscaleEffect.h"
#include <cassert>

GrayscaleEffect::GrayscaleEffect()
{
    // デフォルトパラメータの設定
    params_.intensity = 1.0f;
	params_.enabled = 0;  // エフェクトは無効
	isDirty_ = true;  // 初期状態ではパラメータが変更されているとみなす
}

GrayscaleEffect::~GrayscaleEffect()
{
    
}

void GrayscaleEffect::ApplyEffect(PostEffectParams& params)
{
	if (enabled_)
	{
		params.grayscaleEnabled = 1; // グレースケールエフェクトを有効にする
		params.grayscaleIntensity = params_.intensity;
	}
	else
	{
		params.grayscaleEnabled = 0; // グレースケールエフェクトを無効にする
	}
}

void GrayscaleEffect::SetIntensity(float intensity)
{
    // 値が変更された場合のみ更新フラグを立てる
    if (params_.intensity != intensity)
    {
        params_.intensity = intensity;
        isDirty_ = true;  // 基底クラスのフラグを使用
    }
}

void GrayscaleEffect::SetEnabled(bool enabled)
{
	if (enabled_ != enabled)
	{
		enabled_ = enabled;
		isDirty_ = true;  // 基底クラスのフラグを使用
	}
}
