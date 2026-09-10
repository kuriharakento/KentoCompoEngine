// アウトライン（画面空間の輪郭検出）
// G-Buffer の深度と法線の段差から輪郭を見つけ、線の色を半透明で重ねる。
// 線を引くかどうかは、素材ごとの outlineStrength（発光 RT のアルファ）で決める。
#include "PostEffect.hlsli"

// C++ 側の OutlineRenderer::ConstantsForGPU と一致させること
cbuffer OutlineConstants : register(b0)
{
    row_major float4x4 invProjection;
    float2 screenSize;
    float outlineWidth;
    float depthThreshold;
    float3 outlineColor;
    float normalThreshold;
};

// ライトパスと同じ並び
Texture2D<float4> gAlbedo : register(t0);
Texture2D<float4> gNormal : register(t1);
Texture2D<float4> gMaterial : register(t2);
Texture2D<float4> gEmissive : register(t3);
Texture2D<float> gDepth : register(t4);
SamplerState gPointSampler : register(s0);

// 深度からビュー空間の奥行きを求める（比較を距離に依存させないため）
float ViewDepth(float2 uv)
{
    float depth = gDepth.Sample(gPointSampler, uv);
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 view = mul(float4(ndc, depth, 1.0f), invProjection);
    return view.z / view.w;
}

float3 DecodeNormal(float2 uv)
{
    return normalize(gNormal.Sample(gPointSampler, uv).rgb * 2.0f - 1.0f);
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float2 texel = outlineWidth / screenSize;

    static const float2 kOffsets[4] = { float2(1, 0), float2(-1, 0), float2(0, 1), float2(0, -1) };

    // 線を引く素材が近くにあるか。背景との境界にも線が出るよう、周囲も見る
    float mask = gEmissive.Sample(gPointSampler, uv).a;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        mask = max(mask, gEmissive.Sample(gPointSampler, uv + kOffsets[i] * texel).a);
    }
    if (mask <= 0.0f)
    {
        discard;
    }

    float centerDepth = ViewDepth(uv);
    bool centerIsSky = gDepth.Sample(gPointSampler, uv) >= 1.0f;
    float3 centerNormal = DecodeNormal(uv);

    float edge = 0.0f;
    [unroll]
    for (int j = 0; j < 4; ++j)
    {
        float2 neighborUv = uv + kOffsets[j] * texel;
        bool neighborIsSky = gDepth.Sample(gPointSampler, neighborUv) >= 1.0f;

        // シルエット: 片方だけが空なら確実に輪郭
        if (centerIsSky != neighborIsSky)
        {
            edge = 1.0f;
            continue;
        }
        if (centerIsSky)
        {
            continue;
        }

        // 奥行きの段差（手前のものの奥行きに対する割合で見る。遠くほど差が大きくなるのを打ち消す）
        float neighborDepth = ViewDepth(neighborUv);
        float relative = abs(neighborDepth - centerDepth) / max(min(neighborDepth, centerDepth), 0.001f);
        edge = max(edge, step(depthThreshold, relative));

        // 折れ目（面の向きの差）
        float normalDot = dot(centerNormal, DecodeNormal(neighborUv));
        edge = max(edge, step(normalDot, normalThreshold));
    }

    return float4(outlineColor, saturate(edge * mask));
}
