#include "Sprite.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
    float shininess;
    float reflectivity;
    float2 materialPadding;
    float4 emissiveColorIntensity;
    uint emissiveEnabled;
    uint emissiveSource;
    float bloomContribution;
    float emissivePadding;
};

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
    float32_t4 ambient;
};


ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t4> gEmissiveTexture : register(t1);
SamplerState gSampler : register(s0);
struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
#ifndef KCE_SPRITE_BLOOM_DISABLED
    float32_t4 bloom : SV_TARGET1;
#endif
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float2 transformedUV = mul(float32_t4(input.texcoord,0.0f, 1.0f), gMaterial.uvTransform).xy;
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV);
	float finalAlpha = saturate(gMaterial.color.a * textureColor.a);
#ifndef KCE_SPRITE_BLOOM_DISABLED
	float3 mask = gMaterial.emissiveSource == 0 ? 1.0f.xxx :
		(gMaterial.emissiveSource == 1 ? textureColor.rgb : gEmissiveTexture.Sample(gSampler, transformedUV).rgb);
	float3 emission = mask * gMaterial.emissiveColorIntensity.rgb * gMaterial.emissiveColorIntensity.a;
	if (!all(isfinite(emission)) || !isfinite(gMaterial.bloomContribution)) emission = 0.0f;
	output.bloom = gMaterial.emissiveEnabled != 0 ? float4(clamp(emission, 0.0f, 64.0f) * finalAlpha * saturate(gMaterial.bloomContribution), finalAlpha) : 0.0f;
#endif
    //テクスチャの透明度が0以下の場合は描画しない
	if(textureColor.a == 0.0)
    {
        discard;
    }
    //ライティングを有効にしている場合はライティングを適用
	if(gMaterial.enableLighting != 0)
    {
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        output.color.rgb = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        output.color.a = gMaterial.color.a * textureColor.a;
    }else

    {
        output.color = gMaterial.color * textureColor;
    }
    //Output.colorが透明度0以下の場合は描画しない
    if (output.color.a == 0.0)
    {
        discard;
    }

    return output;
}
