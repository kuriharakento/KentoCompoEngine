#include "Particle.hlsli"

struct Material
{
    float4 color;
    int enableLighting;
    float4x4 uvTransform;
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
    float4 color;
    float3 direction;
    float intensity;
    float4 ambient;
};


ConstantBuffer<Material> gMaterial : register(b0);
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
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

	float finalAlpha = saturate(gMaterial.color.a * textureColor.a * input.color.a);
	if (!isfinite(finalAlpha) || finalAlpha <= 0.001f)
    {
        discard;
    }
	output.color = saturate(gMaterial.color * textureColor * input.color);
	output.color.a = finalAlpha;
#ifndef KCE_BLOOM_TARGET_DISABLED
	float3 emissiveMask = gMaterial.emissiveSource == 0 ? 1.0f.xxx :
		(gMaterial.emissiveSource == 1 ? textureColor.rgb : gEmissiveTexture.Sample(gSampler, transformedUV.xy).rgb);
	float3 emission = emissiveMask * gMaterial.emissiveColorIntensity.rgb * gMaterial.emissiveColorIntensity.a;
	if (!all(isfinite(emission)) || !isfinite(gMaterial.bloomContribution))
	{
		emission = 0.0f;
	}
	emission = clamp(emission, 0.0f, 64.0f);
	float contribution = saturate(gMaterial.bloomContribution);
	output.bloom = gMaterial.emissiveEnabled != 0
		? float4(emission * finalAlpha * contribution, finalAlpha)
		: 0.0f;
#endif

    return output;
}
