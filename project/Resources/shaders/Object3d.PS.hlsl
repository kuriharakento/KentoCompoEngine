#include "Object3d.hlsli"

// マテリアル
struct Material
{
    float4 color;
    int enableLighting;
    float3 padding;
    float4x4 uvTransform;
    float shininess;
    float reflectivity; // 反射率
    float2 pad2;
};

// ディレクショナルライト
struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

// カメラ
struct Camera
{
    float3 worldPos;
};

// ポイントライト
struct GPUPointLight
{
    float4 color;
    float3 position;
    float intensity;
    float radius;
    float decay;
};

// スポットライト
struct GPUSpotLight
{
    float4 color;
    float3 position;
    float intensity;
    float3 direction;
    float distance;
    float decay;
    float cosAngle;
    float cosFalloffStart;
};

struct LightCounts
{
    uint gPointLightCount;
    uint gSpotLightCount;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
StructuredBuffer<GPUPointLight> gPointLights : register(t3);
StructuredBuffer<GPUSpotLight> gSpotLights : register(t4);
ConstantBuffer<LightCounts> gLightCounts : register(b5);

Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t1);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// 効率化された照明計算関数
float3 CalculateHalfLambert(float3 normal, float3 lightDir)
{
    float NdotL = dot(normal, lightDir);
    NdotL = NdotL * 0.5f + 0.5f;
    return pow(NdotL, 2.0f);
}

float3 CalculateSpecular(float3 normal, float3 lightDir, float3 toEye, float3 lightColor, float intensity, float shininess)
{
    float3 halfVector = normalize(lightDir + toEye);
    float NdotH = saturate(dot(normal, halfVector));
    float specularPow = pow(NdotH, shininess);
    return lightColor * intensity * specularPow;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // テクスチャUVとカラーの取得（early out用に先に計算）
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    // アルファテスト（early out）
    if (textureColor.a <= 0.5f)
    {
        discard;
    }

    // ライティングが無効な場合の早期リターン
    if (gMaterial.enableLighting == 0)
    {
        output.color = gMaterial.color * textureColor;
        return output;
    }

    // 共通計算（一度だけ実行）
    float3 normal = normalize(input.normal);
    float3 toEye = normalize(gCamera.worldPos - input.worldPos);
    float3 baseColor = gMaterial.color.rgb * textureColor.rgb;

    /*-----[ ディレクショナルライト ]-----*/
    float3 lightDir = normalize(-gDirectionalLight.direction);
    float NdotL = CalculateHalfLambert(normal, lightDir);
    float3 diffuse = baseColor * gDirectionalLight.color.rgb * NdotL * gDirectionalLight.intensity;
    float3 specular = CalculateSpecular(normal, lightDir, toEye, gDirectionalLight.color.rgb, gDirectionalLight.intensity, gMaterial.shininess);

    /*-----[ ポイントライトの合計（最適化されたループ）]-----*/
    float3 totalPointDiffuse = 0.0f;
    float3 totalPointSpecular = 0.0f;
    
    [loop]
    for (uint j = 0; j < gLightCounts.gPointLightCount; j++)
    {
        float3 lightToPixel = input.worldPos - gPointLights[j].position;
        float distance = length(lightToPixel);
        float3 pointLightDir = lightToPixel / distance; // 正規化を効率化
        
        // 減衰計算
        float factor = pow(saturate(1.0f - distance / gPointLights[j].radius), gPointLights[j].decay);
        
        // 減衰が十分小さい場合はスキップ
        if (factor < 0.01f)
            continue;
        
        float pointNdotL = CalculateHalfLambert(normal, -pointLightDir);
        float3 pointDiffuse = baseColor * gPointLights[j].color.rgb * pointNdotL * gPointLights[j].intensity * factor;
        float3 pointSpecular = CalculateSpecular(normal, -pointLightDir, toEye, gPointLights[j].color.rgb, gPointLights[j].intensity, gMaterial.shininess) * factor;
        
        totalPointDiffuse += pointDiffuse;
        totalPointSpecular += pointSpecular;
    }

    /*-----[ スポットライトの合計（最適化されたループ）]-----*/
    float3 totalSpotDiffuse = 0.0f;
    float3 totalSpotSpecular = 0.0f;
    
    [loop]
    for (uint k = 0; k < gLightCounts.gSpotLightCount; k++)
    {
        float3 lightToPixel = input.worldPos - gSpotLights[k].position;
        float distance = length(lightToPixel);
        float3 spotLightDir = lightToPixel / distance; // 正規化を効率化
        
        // 距離減衰
        float spotFactor = pow(saturate(1.0f - distance / gSpotLights[k].distance), gSpotLights[k].decay);
        
        // フォールオフ計算
        float cosAngle = dot(spotLightDir, gSpotLights[k].direction);
        float falloffFactor = saturate((cosAngle - gSpotLights[k].cosAngle) / (gSpotLights[k].cosFalloffStart - gSpotLights[k].cosAngle));
        
        float combinedFactor = spotFactor * falloffFactor;
        
        // 結合された減衰が十分小さい場合はスキップ
        if (combinedFactor < 0.01f)
            continue;
        
        float spotNdotL = CalculateHalfLambert(normal, -spotLightDir);
        float3 spotDiffuse = baseColor * gSpotLights[k].color.rgb * spotNdotL * gSpotLights[k].intensity * combinedFactor;
        float3 spotSpecular = CalculateSpecular(normal, -spotLightDir, toEye, gSpotLights[k].color.rgb, gSpotLights[k].intensity, gMaterial.shininess) * combinedFactor;
        
        totalSpotDiffuse += spotDiffuse;
        totalSpotSpecular += spotSpecular;
    }

    // ライティング結果の合成
    float3 litColor = diffuse + specular + totalPointDiffuse + totalPointSpecular + totalSpotDiffuse + totalSpotSpecular;

    // 環境マッピング（reflectivityが0でない場合のみ）
    float3 finalColor = litColor;
    if (gMaterial.reflectivity > 0.0f)
    {
        float3 reflectDir = reflect(-toEye, normal);
        float3 envColor = gEnvironmentTexture.Sample(gSampler, reflectDir).rgb;
        finalColor = lerp(litColor, envColor, gMaterial.reflectivity);
    }

    output.color.rgb = finalColor;
    output.color.a = gMaterial.color.a * textureColor.a;

    return output;
}