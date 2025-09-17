#include "PostEffect.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer BlurParams : register(b0)
{
    float2 texelSize;
    float2 blurDirection;
    float radius;
    float padding[3];
}

// 13タップガウシアンブラー
static const int SAMPLE_COUNT = 13;
static const float GAUSSIAN_WEIGHTS[SAMPLE_COUNT] =
{
    0.0044, 0.0175, 0.0540, 0.1295, 0.2420, 0.3521, 0.3989,
    0.3521, 0.2420, 0.1295, 0.0540, 0.0175, 0.0044
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float3 result = 0;
    
    float2 offset = blurDirection * texelSize * radius;
    
    [unroll]
    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        float sampleOffset = (i - SAMPLE_COUNT / 2);
        float2 sampleUV = input.texcoord + offset * sampleOffset;
        
        float3 sampleColor = gTexture.Sample(gSampler, saturate(sampleUV)).rgb;
        result += sampleColor * GAUSSIAN_WEIGHTS[i];
    }
    
    output.color = float4(result, 1.0);
    return output;
}