// 被写界深度
// カメラからの距離でぼけの半径（CoC）を決め、黄金角のらせんで周りを集めて円形にぼかす。
#include "PostEffect.hlsli"

// C++ 側の DepthOfFieldRenderer::ConstantsForGPU と一致させること
cbuffer DofConstants : register(b0)
{
    row_major float4x4 invViewProjection;
    float3 cameraPosition;
    float focusDistance;
    float focusRange;
    float maxBlurPixels;
    float2 invTextureSize;
};

Texture2D<float4> gScene : register(t0);
Texture2D<float> gDepth : register(t1);
SamplerState gLinearSampler : register(s0);
SamplerState gPointSampler : register(s1);

static const int kSampleCount = 32;
// 黄金角（ラジアン）。らせん状に並べるとまんべんなく散らばる
static const float kGoldenAngle = 2.39996323f;
// 何も描かれていない所（空）は十分遠いものとして扱う
static const float kSkyDistance = 1.0e4f;
static const float kMinFocusRange = 1.0e-3f;

float DistanceAt(float2 uv)
{
    float depth = gDepth.SampleLevel(gPointSampler, uv, 0);
    if (depth >= 1.0f)
    {
        return kSkyDistance;
    }
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 world = mul(float4(ndc, depth, 1.0f), invViewProjection);
    world.xyz /= world.w;
    return length(world.xyz - cameraPosition);
}

// ピントの幅の外に出た分だけ、幅1つ分で最大までぼける
float CocPixels(float distanceToCamera)
{
    float outOfFocus = abs(distanceToCamera - focusDistance) - focusRange * 0.5f;
    return saturate(outOfFocus / max(focusRange, kMinFocusRange)) * maxBlurPixels;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float4 center = gScene.SampleLevel(gPointSampler, uv, 0);
    float centerDistance = DistanceAt(uv);
    float centerCoc = CocPixels(centerDistance);

    float3 sum = center.rgb;
    float weightSum = 1.0f;

    [loop]
    for (int i = 0; i < kSampleCount; ++i)
    {
        float radius = sqrt((i + 0.5f) / kSampleCount) * maxBlurPixels;
        float angle = i * kGoldenAngle;
        float2 sampleUv = uv + float2(cos(angle), sin(angle)) * radius * invTextureSize;

        float sampleDistance = DistanceAt(sampleUv);
        float sampleCoc = CocPixels(sampleDistance);
        // 奥にあるものは、手前のピントの合った物の上へにじませない
        if (sampleDistance > centerDistance)
        {
            sampleCoc = min(sampleCoc, centerCoc);
        }
        // その点のぼけの円がここまで届いているときだけ混ぜる
        float weight = saturate(sampleCoc - radius + 1.0f);
        sum += gScene.SampleLevel(gLinearSampler, sampleUv, 0).rgb * weight;
        weightSum += weight;
    }
    return float4(sum / weightSum, center.a);
}
