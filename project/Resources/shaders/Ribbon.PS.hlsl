// Ribbon.PS.hlsl
// リボンレンダラー用のピクセルシェーダー
// Premultiplied Alpha対応

#include "Ribbon.hlsli"

// マテリアル定数バッファ
cbuffer Material : register(b0)
{
    float4 materialColor;
    int enableLighting;
    int useTextureColor;
    float2 materialPadding;
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
    
    // 頂点アルファを取得
    float vertexAlpha = input.color.a;
    
    if (useTextureColor)
    {
        // テクスチャカラーも使用
        output.color.rgb = materialColor.rgb * textureColor.rgb * input.color.rgb;
        output.color.a = materialColor.a * textureColor.a * vertexAlpha;
    }
    else
    {
        // 頂点カラーベース、テクスチャのアルファのみ使用
        output.color.rgb = input.color.rgb * materialColor.rgb;
        output.color.a = vertexAlpha * materialColor.a * textureColor.a;
    }
    
    // アルファが非常に低い場合は破棄
    if (output.color.a < 0.01)
    {
        discard;
    }
    
    // ★ Premultiplied Alpha: RGBにアルファを事前乗算
    // これが黒縁問題を解決する最重要ポイント
    output.color.rgb *= output.color.a;
    
    return output;
}
