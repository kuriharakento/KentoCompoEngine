// Ribbon.PS.hlsl
// リボンレンダラー用のピクセルシェーダー

#include "Ribbon.hlsli"

// マテリアル定数バッファ
cbuffer Material : register(b0)
{
    float4 materialColor;
    int enableLighting;
    float4x4 uvTransform;
}

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // UV変換
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // アルファテスト
    if (textureColor.a == 0.0)
    {
        discard;
    }
    
    // 頂点カラーとテクスチャカラーとマテリアルカラーを合成
    output.color = materialColor * textureColor * input.color;
    
    return output;
}
