// ボリュメトリックの共通定義
// C++ 側の VolumetricLightRenderer::LightForGPU / ConstantsForGPU と一致させること
#include "PostEffect.hlsli"

#define VOLUMETRIC_MAX_LIGHTS 4

struct VolumeLight
{
    float3 position;
    float range;
    float3 direction;
    float cosAngle;
    float3 color;
    float cosFalloffStart;
    float decay;
    int shadowEnabled;
    float2 lightPadding;
    row_major float4x4 shadowViewProj;
};

cbuffer VolumetricConstants : register(b0)
{
    row_major float4x4 invViewProjection;
    float3 cameraPosition;
    float density;
    float anisotropy;
    float maxDistance;
    int stepCount;
    int lightCount;
    float2 invFullSize;
    float2 invHalfSize;
    VolumeLight lights[VOLUMETRIC_MAX_LIGHTS];
};

Texture2D<float> gDepth : register(t0);
SamplerState gLinearSampler : register(s0);
SamplerState gPointSampler : register(s1);

// 何も描かれていない所（空）は十分遠いものとして扱う
static const float kSkyDistance = 1.0e4f;

float3 WorldPositionAt(float2 uv, float depth)
{
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 world = mul(float4(ndc, depth, 1.0f), invViewProjection);
    return world.xyz / world.w;
}
