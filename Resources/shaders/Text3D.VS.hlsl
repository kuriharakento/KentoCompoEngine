// 3D 文字: 1文字1枚の板をインスタンスで並べる
#include "Text3D.hlsli"

// C++ 側の TextMesh3D::Instance と一致させること
struct GlyphInstance
{
    row_major float4x4 world;
    float4 uvRect;
    float4 color;
};

cbuffer Text3DCamera : register(b0)
{
    row_major float4x4 viewProjection;
};

StructuredBuffer<GlyphInstance> gInstances : register(t0);

// 中心が原点の 1x1 の板（三角形2枚）
static const float2 kCorners[6] =
{
    float2(-0.5f, 0.5f), float2(0.5f, 0.5f), float2(-0.5f, -0.5f),
    float2(-0.5f, -0.5f), float2(0.5f, 0.5f), float2(0.5f, -0.5f)
};
static const float2 kTexcoords[6] =
{
    float2(0.0f, 0.0f), float2(1.0f, 0.0f), float2(0.0f, 1.0f),
    float2(0.0f, 1.0f), float2(1.0f, 0.0f), float2(1.0f, 1.0f)
};

Text3DVertexOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    GlyphInstance glyph = gInstances[instanceId];
    float4 world = mul(float4(kCorners[vertexId], 0.0f, 1.0f), glyph.world);

    Text3DVertexOutput output;
    output.position = mul(world, viewProjection);
    output.texcoord = lerp(glyph.uvRect.xy, glyph.uvRect.zw, kTexcoords[vertexId]);
    output.color = glyph.color;
    return output;
}
