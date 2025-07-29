#pragma once
#include "effects/postprocess/IPostEffect.h"
#include "math/Vector3.h"

struct alignas(16) PostEffectParams
{
    // --- グレースケール ---
    float grayscaleIntensity;
    int grayscaleEnabled;
    float pad0[2];

    // --- ヴィネット ---
    int vignetteEnabled;
    float vignetteIntensity;
    float vignetteRadius;
    float vignetteSoftness;

    Vector3 vignetteColor;
    float pad1;

    // --- ノイズ ---
    int noiseEnabled;
    float noiseIntensity;
    float noiseTime;
    float grainSize;

    float luminanceAffect;
    float pad2[3];

    //// --- CRT ---
    int crtEnabled;
    int scanlineEnabled;
    float scanlineIntensity;
    float scanlineCount;

    int distortionEnabled;
    float distortionStrength;
    int chromAberrationEnabled;
    float chromAberrationOffset;

    float pad3[4];

    bool operator==(const PostEffectParams& other) const
    {
        return grayscaleIntensity == other.grayscaleIntensity &&
            grayscaleEnabled == other.grayscaleEnabled &&
            vignetteEnabled == other.vignetteEnabled &&
            vignetteIntensity == other.vignetteIntensity &&
            vignetteRadius == other.vignetteRadius &&
            vignetteSoftness == other.vignetteSoftness &&
            vignetteColor == other.vignetteColor &&
            noiseEnabled == other.noiseEnabled &&
            noiseIntensity == other.noiseIntensity &&
            noiseTime == other.noiseTime &&
            grainSize == other.grainSize &&
            luminanceAffect == other.luminanceAffect &&
            crtEnabled == other.crtEnabled &&
            scanlineEnabled == other.scanlineEnabled &&
            scanlineIntensity == other.scanlineIntensity &&
            scanlineCount == other.scanlineCount &&
            distortionEnabled == other.distortionEnabled &&
            distortionStrength == other.distortionStrength &&
            chromAberrationEnabled == other.chromAberrationEnabled &&
            chromAberrationOffset == other.chromAberrationOffset;
    }

    bool operator!=(const PostEffectParams& other) const
    {
        return !(*this == other);
    }
};

// 基本的な機能を提供する抽象基底クラス
class BasePostEffect : public IPostEffect
{
public:
    BasePostEffect();
    virtual ~BasePostEffect();

    virtual void ApplyEffect(PostEffectParams& params) = 0;

    void SetEnabled(bool enabled) override;
    bool IsEnabled() const override { return enabled_; }

protected:
	bool enabled_ = false; // エフェクトが有効かどうか (0または1)
    bool isDirty_ = true; // パラメータが変更されたかのフラグ

};