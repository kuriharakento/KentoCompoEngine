// ボリュメトリック: 半分の解像度でカメラの光線を進め、空気中で散乱したスポットライトの光を積む
#include "Volumetric.hlsli"

Texture2D<float> gShadow0 : register(t1);
Texture2D<float> gShadow1 : register(t2);
Texture2D<float> gShadow2 : register(t3);
Texture2D<float> gShadow3 : register(t4);
SamplerComparisonState gShadowSampler : register(s2);

static const float kPi = 3.14159265f;
static const int kMaxSteps = 128;
// 空気中の点にはライトに向いた面が無いので、固定の小さなずらしで自己遮蔽を防ぐ
static const float kShadowBias = 0.002f;
static const float kMinLength = 1.0e-4f;

float ShadowAt(int index, float3 worldPosition)
{
    float4 lightSpace = mul(float4(worldPosition, 1.0f), lights[index].shadowViewProj);
    lightSpace.xyz /= lightSpace.w;
    float2 shadowUv = lightSpace.xy * 0.5f + 0.5f;
    shadowUv.y = 1.0f - shadowUv.y;
    if (lightSpace.z < 0.0f || lightSpace.z > 1.0f)
    {
        return 1.0f;
    }
    // シャドウマップの外はサンプラーの境界色（白）で影なしになる
    float compare = lightSpace.z - kShadowBias;
    if (index == 0) return gShadow0.SampleCmpLevelZero(gShadowSampler, shadowUv, compare);
    if (index == 1) return gShadow1.SampleCmpLevelZero(gShadowSampler, shadowUv, compare);
    if (index == 2) return gShadow2.SampleCmpLevelZero(gShadowSampler, shadowUv, compare);
    return gShadow3.SampleCmpLevelZero(gShadowSampler, shadowUv, compare);
}

// Henyey-Greenstein。cosTheta は光の進む向きと、カメラへ向かう向きのなす角
float Phase(float cosTheta)
{
    float g = anisotropy;
    float g2 = g * g;
    return (1.0f - g2) / (4.0f * kPi * pow(max(1.0f + g2 - 2.0f * g * cosTheta, kMinLength), 1.5f));
}

// インターリーブド・グラディエント・ノイズ。歩き始めをピクセルごとにずらして、段々の縞をノイズに変える
float Dither(float2 pixel)
{
    return frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float depth = gDepth.SampleLevel(gPointSampler, uv, 0);
    float3 target = WorldPositionAt(uv, depth);

    float3 ray = target - cameraPosition;
    float rayLength = min(length(ray), maxDistance);
    float3 direction = ray / max(length(ray), kMinLength);

    int steps = clamp(stepCount, 1, kMaxSteps);
    float stepLength = rayLength / steps;
    float offset = Dither(input.position.xy);

    float3 inscatter = float3(0.0f, 0.0f, 0.0f);
    float transmittance = 1.0f;

    [loop]
    for (int i = 0; i < steps; ++i)
    {
        float3 samplePosition = cameraPosition + direction * ((i + offset) * stepLength);
        float3 lighting = float3(0.0f, 0.0f, 0.0f);

        [loop]
        for (int l = 0; l < lightCount; ++l)
        {
            float3 toSample = samplePosition - lights[l].position;
            float distanceToLight = length(toSample);
            if (distanceToLight >= lights[l].range)
            {
                continue;
            }
            float3 lightDirection = toSample / max(distanceToLight, kMinLength);

            // ライトパスと同じコーンの形と距離の減衰
            float cosToAxis = dot(lightDirection, lights[l].direction);
            float cone = saturate((cosToAxis - lights[l].cosAngle) / max(lights[l].cosFalloffStart - lights[l].cosAngle, kMinLength));
            if (cone <= 0.0f)
            {
                continue;
            }
            float attenuation = pow(saturate(1.0f - distanceToLight / lights[l].range), lights[l].decay);
            float shadow = lights[l].shadowEnabled != 0 ? ShadowAt(l, samplePosition) : 1.0f;
            lighting += lights[l].color * (cone * attenuation * shadow * Phase(dot(lightDirection, -direction)));
        }

        inscatter += transmittance * lighting * density * stepLength;
        transmittance *= exp(-density * stepLength);
    }
    return float4(inscatter, 1.0f);
}
