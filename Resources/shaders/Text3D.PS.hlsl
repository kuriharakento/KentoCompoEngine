// 3D 文字: アトラスの濃さを透明度にして、指定の色で塗る（色は HDR 可）
#include "Text3D.hlsli"

Texture2D<float4> gAtlas : register(t1);
SamplerState gSampler : register(s0);

// ほぼ透明な所は深度の前後関係を乱さないように捨てる
static const float kMinAlpha = 1.0f / 255.0f;

float4 main(Text3DVertexOutput input) : SV_TARGET
{
    float alpha = gAtlas.Sample(gSampler, input.texcoord).a * input.color.a;
    if (alpha <= kMinAlpha)
    {
        discard;
    }
    return float4(input.color.rgb, alpha);
}
