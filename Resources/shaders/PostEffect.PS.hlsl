#include "PostEffect.hlsli"

Texture2D<float4> gTexture : register(t0); // メインシーン
Texture2D<float4> gBloomTexture : register(t1); // ブルーム
SamplerState gSampler : register(s0);

cbuffer PostEffectParams : register(b0)
{
    // Grayscale
    float grayscaleIntensity;
    int grayscaleEnabled;
    float2 pad0;

    // Vignette
    int vignetteEnabled;
    float vignetteIntensity;
    float vignetteRadius;
    float vignetteSoftness;
    float3 vignetteColor;
    float pad1;

    // Noise
    int noiseEnabled;
    float noiseIntensity;
    float noiseTime;
    float grainSize;
    float luminanceAffect;
    float3 pad2;

    // CRT
    int crtEnabled;
    int scanlineEnabled;
    float scanlineIntensity;
    float scanlineCount;
    int distortionEnabled;
    float distortionStrength;
    int chromAberrationEnabled;
    float chromAberrationOffset;
    float4 pad3;

    // Bloom
    int bloomEnabled;
    float bloomIntensity;
    float bloomThreshold;
    float bloomRadius;
    float3 pad4;
    float2 invScreenSize;
    float bloomThresholdKnee;
    float bloomMix;

    // Tonemap
    int tonemapEnabled;
    float tonemapExposure;
    int tonemapMode;
    float pad5;

    // Gaussian blur
    int gaussianBlurEnabled;
    float gaussianBlurRadius;
    float gaussianBlurStrength;
    float pad6;

    // Diffusion
    int diffusionEnabled;
    float diffusionRadius;
    float diffusionIntensity;
    float pad7;

    // Radial blur
    int radialBlurEnabled;
    int radialBlurSampleCount;
    float2 radialBlurCenter;
    float radialBlurStrength;
    float radialBlurBlend;
    float2 pad8;

    // Color grading
    int colorGradingEnabled;
    float3 pad9;
    float3 colorGradingLift;
    float pad10;
    float3 colorGradingGamma;
    float pad11;
    float3 colorGradingGain;
    float pad12;
    float colorGradingSaturation;
    float colorGradingContrast;
    float2 pad13;
};

static const int kMaxRadialSamples = 32;
static const float kMinimumGamma = 0.001;

float3 sampleSoftBlur(float2 uv, float radius)
{
    const float2 texel = invScreenSize * radius;
    float3 result = gTexture.Sample(gSampler, uv).rgb * 0.227027;
    result += gTexture.Sample(gSampler, saturate(uv + float2(texel.x, 0.0))).rgb * 0.158108;
    result += gTexture.Sample(gSampler, saturate(uv - float2(texel.x, 0.0))).rgb * 0.158108;
    result += gTexture.Sample(gSampler, saturate(uv + float2(0.0, texel.y))).rgb * 0.158108;
    result += gTexture.Sample(gSampler, saturate(uv - float2(0.0, texel.y))).rgb * 0.158108;
    result += gTexture.Sample(gSampler, saturate(uv + texel)).rgb * 0.035635;
    result += gTexture.Sample(gSampler, saturate(uv - texel)).rgb * 0.035635;
    result += gTexture.Sample(gSampler, saturate(uv + float2(texel.x, -texel.y))).rgb * 0.035635;
    result += gTexture.Sample(gSampler, saturate(uv + float2(-texel.x, texel.y))).rgb * 0.035635;
    return result;
}

// ACES のフィルミックカーブ近似（Krzysztof Narkowicz）
// ハイライトの立ち上がりが自然で、白飽和したときの色転びが少ない
float3 tonemapACES(float3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

// Reinhard。素直だがハイライトが眠くなりやすい
float3 tonemapReinhard(float3 color)
{
    return color / (1.0 + color);
}

// 最適化されたランダム関数
float fastRandom(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453 + noiseTime * 0.1);
}

// 歪み適用（常に計算、係数で制御）
float2 applyDistortion(float2 uv, float strength)
{
    float2 offset = uv - 0.5;
    return uv + offset * strength * dot(offset, offset);
}

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    float2 uv = input.texcoord;
    
    // フラグを正規化（0.0 or 1.0）しておくのはそのまま
    float crtFactor = saturate((float) crtEnabled);
    
    // 変数を宣言（初期値はベースカラー取得までやる）
    float4 baseColor = gTexture.Sample(gSampler, uv);
    float3 color = baseColor.rgb;

    // 歪み（重いので条件付き）
    if (crtEnabled != 0 && distortionEnabled != 0)
    {
        float2 distortedUV = applyDistortion(uv, distortionStrength);
        uv = lerp(uv, distortedUV, 1.0);
        baseColor = gTexture.Sample(gSampler, uv);
        color = baseColor.rgb;
    }
    
    // 色収差（条件付き）
    if (crtEnabled != 0 && chromAberrationEnabled != 0)
    {
        float2 chromOffset = chromAberrationOffset * 0.001;
        float r = gTexture.Sample(gSampler, uv + chromOffset).r;
        float b = gTexture.Sample(gSampler, uv - chromOffset).b;
        float3 chromColor = float3(r, baseColor.g, b);
        color = lerp(color, chromColor, 1.0);
    }

    if (gaussianBlurEnabled != 0)
    {
        float3 blurred = sampleSoftBlur(uv, gaussianBlurRadius);
        color = lerp(color, blurred, saturate(gaussianBlurStrength));
    }

    if (diffusionEnabled != 0)
    {
        float3 diffused = sampleSoftBlur(uv, diffusionRadius);
        float3 screened = 1.0 - (1.0 - color) * (1.0 - diffused);
        color = lerp(color, screened, saturate(diffusionIntensity));
    }

    if (radialBlurEnabled != 0)
    {
        int sampleCount = clamp(radialBlurSampleCount, 2, kMaxRadialSamples);
        float2 direction = uv - radialBlurCenter;
        // 中心のサンプルはここまでの結果を使う。元テクスチャから取り直すと、
        // 歪みやぼかしの結果を捨ててしまう
        float3 radialColor = color;
        [loop]
        for (int sampleIndex = 1; sampleIndex < sampleCount; ++sampleIndex)
        {
            float progress = (float)sampleIndex / (float)(sampleCount - 1);
            float2 sampleUV = saturate(uv - direction * radialBlurStrength * progress);
            radialColor += gTexture.Sample(gSampler, sampleUV).rgb;
        }
        radialColor /= (float)sampleCount;
        // ずらした先は元テクスチャから引くので、前段の結果が乗るのは中心分だけ。
        // 1パスで完結させている都合の割り切り
        color = lerp(color, radialColor, saturate(radialBlurBlend));
    }
    
    // 走査線（条件付き）
    if (crtEnabled != 0 && scanlineEnabled != 0)
    {
        float scanlinePattern = 1.0 - scanlineIntensity * 0.5 * (1.0 + sin(uv.y * scanlineCount * 6.283185));
        color *= scanlinePattern;
    }
    
    // ノイズ（条件付き）
    if (noiseEnabled != 0)
    {
        float2 grainUV = uv * grainSize + noiseTime;
        float noiseValue = fastRandom(grainUV);
        float luminance = dot(color, float3(0.299, 0.587, 0.114));
        float luminanceFactor = lerp(1.0, luminance, luminanceAffect);
        float finalNoise = (noiseValue - 0.5) * noiseIntensity * luminanceFactor;
        color += finalNoise;
    }
    
    // ビネット（条件付き）
    if (vignetteEnabled != 0)
    {
        float2 vignetteOffset = uv - 0.5;
        float dist = length(vignetteOffset);
        float vignetteEdge = vignetteRadius - vignetteSoftness;
        float vignetteStrength = saturate((dist - vignetteEdge) / vignetteSoftness);
        float3 vignetteResult = lerp(color, vignetteColor, vignetteStrength * vignetteIntensity);
        color = lerp(color, vignetteResult, 1.0);
    }
    
    // グレースケール（条件付き）
    if (grayscaleEnabled != 0)
    {
        float finalLuminance = dot(color, float3(0.2126, 0.7152, 0.0722));
        float3 grayscaleResult = lerp(color, finalLuminance.xxx, grayscaleIntensity);
        color = lerp(color, grayscaleResult, 1.0);
    }
    
    // Bloom処理部分
    if (bloomEnabled != 0)
    {
        float3 bloom = gBloomTexture.Sample(gSampler, uv).rgb;
        color += bloom * bloomIntensity;
    }
    
    // トーンマップ（HDR → LDR）
    // メインRTがHDRになったため、ここを通らないと1.0を超えた輝度がそのまま出る。
    // 出力先はsRGBフォーマットなので、リニアのまま書き込めばよい。
    if (tonemapEnabled != 0)
    {
        color *= tonemapExposure;
        color = (tonemapMode == 0) ? tonemapACES(color) : tonemapReinhard(color);
    }

    // カラーグレーディングはLDRへ落とした後に適用する。
    if (colorGradingEnabled != 0)
    {
        color = max(color + colorGradingLift, 0.0);
        color = pow(color, rcp(max(colorGradingGamma, kMinimumGamma.xxx)));
        color *= colorGradingGain;
        float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
        color = lerp(luminance.xxx, color, colorGradingSaturation);
        color = (color - 0.5) * colorGradingContrast + 0.5;
        color = saturate(color);
    }

    PixelShaderOutput output;
    output.color = float4(color, baseColor.a);
    return output;
}
