#pragma once
#include "base/BasePostEffect.h"

class BloomEffect : public BasePostEffect
{
public:
    BloomEffect();
    ~BloomEffect() override;

    void ApplyEffect(PostEffectParams& params) override;

    // Bloom特有のパラメータ
    void SetIntensity(float intensity);
    float GetIntensity() const { return params_.intensity; }
    void SetThreshold(float threshold);
    float GetThreshold() const { return params_.threshold; }
    void SetRadius(float radius);
    float GetRadius() const { return params_.radius; }
    void SetEnabled(bool enabled) override;

private:
    struct Parameters
    {
        int enabled;       // Bloomエフェクト有効フラグ
        float intensity;   // Bloom強度
        float threshold;   // 明るさ抽出のしきい値
        float radius;      // ぼかし半径
        float padding[3];  // 16バイトアラインメント
    };
    Parameters params_;
};

