#include "PostEffect.hlsli"

Texture2D<float4> gTexture : register(t0);
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

    // Bloom base
    int bloomEnabled;
    float bloomIntensity;
    float bloomThreshold;
    float bloomRadius;
    float3 pad4;

    // Bloom extended
    float2 invScreenSize;
    float bloomThresholdKnee;
    float bloomMix;
};

static const float3 LUMA = float3(0.299, 0.587, 0.114);

float fastRandom(float2 uv)
{
    return frac(dot(uv, float2(12.9898, 78.233)) * 43758.5453 + noiseTime * 0.1);
}

float2 applyDistortion(float2 uv, float strength)
{
    float2 d = uv - 0.5;
    return uv + d * strength * dot(d, d);
}

// Soft threshold (knee)
float SoftThresholdFactor(float lum, float threshold, float knee)
{
    float k = max(knee, 1e-5);
    float lower = threshold - k;
    if (lum <= lower)
        return 0.0;
    if (lum >= threshold)
        return (lum - threshold) / max(1.0 - threshold, 1e-5);
    float x = (lum - lower) / (threshold - lower);
    return saturate(x * x * (3 - 2 * x));
}

float3 ApplyBloom(float2 uv)
{
    if (bloomRadius <= 0.0)
        return 0;

    const int SAMPLE_COUNT = 13;
    float2 dirs[SAMPLE_COUNT] =
    {
        float2(0, 0),
        float2(1, 0), float2(-1, 0), float2(0, 1), float2(0, -1),
        float2(1, 1), float2(-1, 1), float2(1, -1), float2(-1, -1),
        float2(0.5, 0), float2(-0.5, 0), float2(0, 0.5), float2(0, -0.5)
    };

    float weights[SAMPLE_COUNT] =
    {
        0.24,
        0.12, 0.12, 0.12, 0.12,
        0.06, 0.06, 0.06, 0.06,
        0.05, 0.05, 0.05, 0.05
    };

    float wsum = 0;
    [unroll]
    for (int i = 0; i < SAMPLE_COUNT; ++i)
        wsum += weights[i];
    float invW = 1.0 / wsum;

    float2 scale = bloomRadius * invScreenSize;
    float3 accum = 0;

    [unroll]
    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        float2 suv = uv + dirs[i] * scale;
        float3 c = gTexture.Sample(gSampler, saturate(suv)).rgb;
        float lum = dot(c, LUMA);
        float f = SoftThresholdFactor(lum, bloomThreshold, bloomThresholdKnee);
        accum += c * (f * weights[i] * invW);
    }

    return accum * bloomIntensity;
}

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput o;
    float2 uv = input.texcoord;
    float4 baseColor = gTexture.Sample(gSampler, uv);
    float3 color = baseColor.rgb;

    // Distortion
    if (crtEnabled != 0 && distortionEnabled != 0)
    {
        uv = applyDistortion(uv, distortionStrength);
        baseColor = gTexture.Sample(gSampler, uv);
        color = baseColor.rgb;
    }
    // Chromatic Aberration
    if (crtEnabled != 0 && chromAberrationEnabled != 0)
    {
        float2 ofs = chromAberrationOffset * 0.001;
        float r = gTexture.Sample(gSampler, uv + ofs).r;
        float b = gTexture.Sample(gSampler, uv - ofs).b;
        color = float3(r, baseColor.g, b);
    }
    // Scanline
    if (crtEnabled != 0 && scanlineEnabled != 0)
    {
        float pat = 1.0 - scanlineIntensity * 0.5 * (1.0 + sin(uv.y * scanlineCount * 6.283185));
        color *= pat;
    }
    // Noise
    if (noiseEnabled != 0)
    {
        float2 grainUV = uv * grainSize + noiseTime;
        float n = fastRandom(grainUV);
        float lum = dot(color, LUMA);
        float lf = lerp(1.0, lum, luminanceAffect);
        color += (n - 0.5) * noiseIntensity * lf;
    }
    // Vignette
    if (vignetteEnabled != 0)
    {
        float2 d = uv - 0.5;
        float dist = length(d);
        float edge = vignetteRadius - vignetteSoftness;
        float t = saturate((dist - edge) / max(vignetteSoftness, 1e-5));
        color = lerp(color, vignetteColor, t * vignetteIntensity);
    }
    // Grayscale
    if (grayscaleEnabled != 0)
    {
        float lum = dot(color, float3(0.2126, 0.7152, 0.0722));
        color = lerp(color, lum.xxx, grayscaleIntensity);
    }
    // Bloom
    if (bloomEnabled != 0)
    {
        float3 bloom = ApplyBloom(input.texcoord);
        float3 composite = color + bloom;
        color = lerp(color, composite, bloomMix);
    }

    o.color = float4(saturate(color), baseColor.a);
    return o;
}