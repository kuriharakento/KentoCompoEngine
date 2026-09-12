// 2D 文字: アトラスの濃さを透明度にして、指定の色で塗る
#include "TextSprite.hlsli"

Texture2D<float4> gAtlas : register(t1);
SamplerState gSampler : register(s0);

float4 main(TextSpriteVertexOutput input) : SV_TARGET
{
    float alpha = gAtlas.Sample(gSampler, input.texcoord).a * input.color.a;
    if (alpha <= 0.0f)
    {
        discard;
    }
    return float4(input.color.rgb, alpha);
}
