// 別のターゲットの絵をそのまま書き写す
#include "PostEffect.hlsli"

Texture2D<float4> gSource : register(t0);
SamplerState gPointSampler : register(s1);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    return gSource.SampleLevel(gPointSampler, input.texcoord, 0);
}
