// Ribbon.PS.hlsl
// リボンレンダラー用のピクセルシェーダー
// Premultiplied Alpha対応（強化版）

#include "Ribbon.hlsli"

// マテリアル定数バッファ
cbuffer Material : register(b0)
{
    float4 materialColor;
    int enableLighting;
    int useTextureColor;
    float2 materialPadding;
    float4x4 uvTransform;
    float4 emissiveColorIntensity;
    uint emissiveEnabled;
    uint emissiveSource;
    float bloomContribution;
    float emissivePadding;
}

Texture2D<float4> gTexture : register(t0);
Texture2D<float4> gEmissiveTexture : register(t1);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
#ifndef KCE_BLOOM_TARGET_DISABLED
	float4 bloom : SV_TARGET1;
#endif
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // UV変換
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // 頂点アルファを取得
    float vertexAlpha = input.color.a;
    
    // 最終アルファを計算
    float finalAlpha = vertexAlpha * materialColor.a * textureColor.a;
    
    // アルファが非常に低い場合は早期に破棄
    if (finalAlpha < 0.001)
    {
        discard;
    }
    
    // ベースカラーを計算
    float3 baseColor;
    if (useTextureColor)
    {
        // テクスチャカラーも使用
        baseColor = materialColor.rgb * textureColor.rgb * input.color.rgb;
    }
    else
    {
        // 頂点カラーベース（テクスチャのアルファのみ使用）
        baseColor = input.color.rgb * materialColor.rgb;
    }
    
    // ★ Premultiplied Alpha出力
    // RGB = baseColor * finalAlpha（事前乗算）
    // A = finalAlpha
    output.color.rgb = baseColor * finalAlpha;
    output.color.a = finalAlpha;
#ifndef KCE_BLOOM_TARGET_DISABLED
	float3 emissiveMask = emissiveSource == 0 ? 1.0f.xxx :
		(emissiveSource == 1 ? textureColor.rgb : gEmissiveTexture.Sample(gSampler, transformedUV.xy).rgb);
	float3 emission = emissiveMask * emissiveColorIntensity.rgb * emissiveColorIntensity.a;
	if (!all(isfinite(emission)) || !isfinite(bloomContribution))
	{
		emission = 0.0f;
	}
	emission = clamp(emission, 0.0f, 64.0f);
	output.bloom = emissiveEnabled != 0
		? float4(emission * finalAlpha * saturate(bloomContribution), finalAlpha)
		: 0.0f;
#endif
    
    return output;
}
