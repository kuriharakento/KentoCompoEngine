// 3D 文字の深度だけを書くパス
// 被写界深度などが文字の距離を読めるように、濃い所だけ深度を書く。
// 色は次のパスで重ねるので、ここでは書かない（出力先の書き込みマスクは 0）
#include "Text3D.hlsli"

Texture2D<float4> gAtlas : register(t1);
SamplerState gSampler : register(s0);

// これより薄い縁は深度を書かない。書くと縁の向こうの物が欠ける
static const float kDepthAlphaThreshold = 0.5f;

float4 main(Text3DVertexOutput input) : SV_TARGET
{
    float alpha = gAtlas.Sample(gSampler, input.texcoord).a * input.color.a;
    if (alpha < kDepthAlphaThreshold)
    {
        discard;
    }
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}
