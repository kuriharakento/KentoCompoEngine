// 大気フォグ
// シーンの深度からワールド座標を復元し、カメラからの距離と高さで霧の濃さを決める。
// 出力のアルファが霧の濃さで、パイプライン側で SrcAlpha / InvSrcAlpha で重ねる。
#include "PostEffect.hlsli"

// C++ 側の FogRenderer::ConstantsForGPU と一致させること
cbuffer FogConstants : register(b0)
{
    row_major float4x4 invViewProjection;
    float3 cameraPosition;
    float density;
    float3 fogColor;
    float heightFalloff;
    float baseHeight;
    float maxOpacity;
    float skyAmount;
    float fogPadding;
};

Texture2D<float> gDepth : register(t0);
SamplerState gPointSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float depth = gDepth.Sample(gPointSampler, uv);

    // 何も描かれていない（空）ピクセルには一定量だけかける
    if (depth >= 1.0f)
    {
        return float4(fogColor, saturate(skyAmount * maxOpacity));
    }

    // 深度からワールド座標を復元する
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 world = mul(float4(ndc, depth, 1.0f), invViewProjection);
    world.xyz /= world.w;

    // 距離による濃さ（指数減衰）
    float distanceToCamera = length(world.xyz - cameraPosition);
    float distanceFog = 1.0f - exp(-density * distanceToCamera);

    // 高さによる薄まり。基準の高さより上ほど霧が薄い（地面に溜まる霧）
    float heightAbove = max(world.y - baseHeight, 0.0f);
    float heightFactor = exp(-heightFalloff * heightAbove);

    float fog = saturate(distanceFog * heightFactor) * maxOpacity;
    return float4(fogColor, fog);
}
