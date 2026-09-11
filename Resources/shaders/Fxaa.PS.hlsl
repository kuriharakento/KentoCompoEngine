// FXAA
// トーンマップ後の絵で、明るさの差が大きい所を縁とみなし、縁に沿って探して
// 段差の位置に応じた分だけ縁と直角の方向へずらして読み直す。
#include "PostEffect.hlsli"

// C++ 側の FxaaRenderer::ConstantsForGPU と一致させること
cbuffer FxaaConstants : register(b0)
{
    float2 invTextureSize;
    float subpixel;
    float edgeThreshold;
    float edgeThresholdMin;
    float3 fxaaPadding;
};

Texture2D<float4> gInput : register(t0);
SamplerState gLinearSampler : register(s0);

// 縁に沿って探す回数と、回ごとに進む幅（遠くほど大きく飛ぶ）
static const int kSearchSteps = 10;
static const float kStepScale[kSearchSteps] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 2.0f, 2.0f, 2.0f, 4.0f, 8.0f };
static const float3 kLumaWeights = float3(0.299f, 0.587f, 0.114f);

// sRGB のターゲットから読むとリニアになるので、平方根で見た目の明るさに近づける
float Luma(float3 color)
{
    return dot(sqrt(max(color, 0.0f)), kLumaWeights);
}

float LumaAt(float2 uv)
{
    return Luma(gInput.SampleLevel(gLinearSampler, uv, 0).rgb);
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float2 px = invTextureSize;
    float4 centerColor = gInput.SampleLevel(gLinearSampler, uv, 0);

    float lumaM = Luma(centerColor.rgb);
    float lumaN = LumaAt(uv + float2(0.0f, -px.y));
    float lumaS = LumaAt(uv + float2(0.0f, px.y));
    float lumaE = LumaAt(uv + float2(px.x, 0.0f));
    float lumaW = LumaAt(uv + float2(-px.x, 0.0f));

    float rangeMax = max(lumaM, max(max(lumaN, lumaS), max(lumaE, lumaW)));
    float rangeMin = min(lumaM, min(min(lumaN, lumaS), min(lumaE, lumaW)));
    float range = rangeMax - rangeMin;
    if (range < max(edgeThresholdMin, rangeMax * edgeThreshold))
    {
        return centerColor;
    }

    float lumaNW = LumaAt(uv + float2(-px.x, -px.y));
    float lumaNE = LumaAt(uv + float2(px.x, -px.y));
    float lumaSW = LumaAt(uv + float2(-px.x, px.y));
    float lumaSE = LumaAt(uv + float2(px.x, px.y));

    // 1ピクセルより細かい段差の分。周りの平均との差が大きいほど強く均す
    float lumaAverage = (lumaN + lumaS + lumaE + lumaW) * 0.25f;
    float subpixelBlend = saturate(abs(lumaAverage - lumaM) / range);
    subpixelBlend = smoothstep(0.0f, 1.0f, subpixelBlend);
    subpixelBlend = subpixelBlend * subpixelBlend * subpixel;

    // 縁が横向きか縦向きか（明るさが変わる向き）
    float edgeHorizontal = abs(0.25f * lumaNW - 0.5f * lumaN + 0.25f * lumaNE)
                         + abs(0.5f * lumaW - lumaM + 0.5f * lumaE)
                         + abs(0.25f * lumaSW - 0.5f * lumaS + 0.25f * lumaSE);
    float edgeVertical = abs(0.25f * lumaNW - 0.5f * lumaW + 0.25f * lumaSW)
                       + abs(0.5f * lumaN - lumaM + 0.5f * lumaS)
                       + abs(0.25f * lumaNE - 0.5f * lumaE + 0.25f * lumaSE);
    bool horizontalSpan = edgeHorizontal >= edgeVertical;

    // 縁のどちら側に段差があるか
    float luma1 = horizontalSpan ? lumaN : lumaW;
    float luma2 = horizontalSpan ? lumaS : lumaE;
    float gradient1 = abs(luma1 - lumaM);
    float gradient2 = abs(luma2 - lumaM);
    bool towardFirst = gradient1 >= gradient2;
    float stepLength = horizontalSpan ? px.y : px.x;
    if (towardFirst)
    {
        stepLength = -stepLength;
    }
    float gradientScaled = max(gradient1, gradient2) * 0.25f;
    float lumaLocalAverage = 0.5f * (towardFirst ? luma1 : luma2) + 0.5f * lumaM;

    // 段差の真ん中の線の上を、縁の両方向へ端まで探す
    float2 edgeUv = uv;
    if (horizontalSpan)
    {
        edgeUv.y += stepLength * 0.5f;
    }
    else
    {
        edgeUv.x += stepLength * 0.5f;
    }
    float2 edgeStep = horizontalSpan ? float2(px.x, 0.0f) : float2(0.0f, px.y);
    float2 uvNegative = edgeUv - edgeStep;
    float2 uvPositive = edgeUv + edgeStep;
    float lumaEndNegative = LumaAt(uvNegative) - lumaLocalAverage;
    float lumaEndPositive = LumaAt(uvPositive) - lumaLocalAverage;
    bool reachedNegative = abs(lumaEndNegative) >= gradientScaled;
    bool reachedPositive = abs(lumaEndPositive) >= gradientScaled;

    [loop]
    for (int i = 0; i < kSearchSteps && !(reachedNegative && reachedPositive); ++i)
    {
        if (!reachedNegative)
        {
            uvNegative -= edgeStep * kStepScale[i];
            lumaEndNegative = LumaAt(uvNegative) - lumaLocalAverage;
            reachedNegative = abs(lumaEndNegative) >= gradientScaled;
        }
        if (!reachedPositive)
        {
            uvPositive += edgeStep * kStepScale[i];
            lumaEndPositive = LumaAt(uvPositive) - lumaLocalAverage;
            reachedPositive = abs(lumaEndPositive) >= gradientScaled;
        }
    }

    float distanceNegative = horizontalSpan ? (uv.x - uvNegative.x) : (uv.y - uvNegative.y);
    float distancePositive = horizontalSpan ? (uvPositive.x - uv.x) : (uvPositive.y - uv.y);
    bool nearerNegative = distanceNegative < distancePositive;
    float nearestDistance = min(distanceNegative, distancePositive);
    float spanLength = distanceNegative + distancePositive;

    // 近い方の端が、真ん中と同じ側の明るさなら段差はここまで来ていない
    bool centerDarker = (lumaM - lumaLocalAverage) < 0.0f;
    bool correctVariation = ((nearerNegative ? lumaEndNegative : lumaEndPositive) < 0.0f) != centerDarker;
    float edgeOffset = correctVariation ? (0.5f - nearestDistance / spanLength) : 0.0f;

    float finalOffset = max(edgeOffset, subpixelBlend);
    float2 finalUv = uv;
    if (horizontalSpan)
    {
        finalUv.y += finalOffset * stepLength;
    }
    else
    {
        finalUv.x += finalOffset * stepLength;
    }
    return float4(gInput.SampleLevel(gLinearSampler, finalUv, 0).rgb, centerColor.a);
}
