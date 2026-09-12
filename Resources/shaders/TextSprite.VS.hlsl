// 2D 文字: 1文字1枚の板をインスタンスで並べる（座標は 1920x1080 の仮想画面）
#include "TextSprite.hlsli"

// C++ 側の TextSpritePipeline::Instance と一致させること
struct GlyphQuad
{
    float4 rect;   // 左上 xy と大きさ zw（仮想画面のピクセル）
    float4 uvRect; // アトラスの UV（左上 xy、右下 zw）
    float4 color;
};

StructuredBuffer<GlyphQuad> gQuads : register(t0);

// Sprite::kCoordinateWidth / kCoordinateHeight と同じ
static const float2 kVirtualScreenSize = float2(1920.0f, 1080.0f);

// 左上が 0、右下が 1 の板（三角形2枚）
static const float2 kCorners[6] =
{
    float2(0.0f, 0.0f), float2(1.0f, 0.0f), float2(0.0f, 1.0f),
    float2(0.0f, 1.0f), float2(1.0f, 0.0f), float2(1.0f, 1.0f)
};

TextSpriteVertexOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    GlyphQuad quad = gQuads[instanceId];
    float2 corner = kCorners[vertexId];
    float2 pixel = quad.rect.xy + corner * quad.rect.zw;

    TextSpriteVertexOutput output;
    // 仮想画面のピクセル → NDC（Y は下向きから上向きへ）
    output.position = float4(pixel.x / kVirtualScreenSize.x * 2.0f - 1.0f, 1.0f - pixel.y / kVirtualScreenSize.y * 2.0f, 0.0f, 1.0f);
    output.texcoord = lerp(quad.uvRect.xy, quad.uvRect.zw, corner);
    output.color = quad.color;
    return output;
}
