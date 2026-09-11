// 床の平面反射の合成
// 深度から位置を戻し、床の高さにあって上を向いているピクセルにだけ、
// 鏡映したカメラの絵を反射率（出力のアルファ）の分だけ混ぜる。
#include "PostEffect.hlsli"

// C++ 側の PlanarReflection::ConstantsForGPU と一致させること
cbuffer ReflectionConstants : register(b0)
{
    row_major float4x4 invViewProjection;
    float3 cameraPosition;
    float planeHeight;
    float strength;
    float fresnelPower;
    float minReflectance;
    float heightTolerance;
};

Texture2D<float> gDepth : register(t0);
Texture2D<float4> gNormal : register(t1);
Texture2D<float4> gReflection : register(t2);
SamplerState gLinearSampler : register(s0);
SamplerState gPointSampler : register(s1);

// これより上を向いていれば床とみなす（法線の Y）
static const float kUpThreshold = 0.9f;

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float depth = gDepth.SampleLevel(gPointSampler, uv, 0);
    if (depth >= 1.0f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 world = mul(float4(ndc, depth, 1.0f), invViewProjection);
    world.xyz /= world.w;
    if (abs(world.y - planeHeight) > heightTolerance)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // G-Buffer の法線は 0〜1 に詰めてある
    float3 normal = normalize(gNormal.SampleLevel(gPointSampler, uv, 0).xyz * 2.0f - 1.0f);
    if (normal.y < kUpThreshold)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // 浅い角度から見るほど強く映る
    float3 toCamera = normalize(cameraPosition - world.xyz);
    float fresnel = pow(1.0f - saturate(toCamera.y), fresnelPower);
    float amount = strength * lerp(minReflectance, 1.0f, fresnel);

    // 鏡映したカメラの像は上下が反転している
    float3 reflection = gReflection.SampleLevel(gLinearSampler, float2(uv.x, 1.0f - uv.y), 0).rgb;
    return float4(reflection, saturate(amount));
}
