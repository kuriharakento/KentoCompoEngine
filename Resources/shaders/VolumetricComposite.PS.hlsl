// ボリュメトリック: 半分の解像度の光の筋を、深度を見ながら拡大してシーンへ足す（バイラテラル）
// 奥行きの違う面どうしで混ぜると、物の輪郭の周りに光がにじむため
#include "Volumetric.hlsli"

Texture2D<float4> gVolume : register(t1);

// 奥行きの差（割合）に対する重みの効き方
static const float kDepthEpsilon = 0.01f;
static const float kMinDistance = 1.0e-3f;
static const float kMinWeight = 1.0e-5f;

float DistanceAt(float2 uv)
{
    float depth = gDepth.SampleLevel(gPointSampler, uv, 0);
    if (depth >= 1.0f)
    {
        return kSkyDistance;
    }
    return length(WorldPositionAt(uv, depth) - cameraPosition);
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float centerDistance = DistanceAt(uv);

    // 周りの半分解像度のテクセル4つを、バイリニアの重みと奥行きの近さで混ぜる
    float2 halfSize = 1.0f / invHalfSize;
    float2 texel = uv * halfSize - 0.5f;
    float2 base = floor(texel);
    float2 fraction = texel - base;

    float3 sum = float3(0.0f, 0.0f, 0.0f);
    float weightSum = 0.0f;
    [unroll]
    for (int y = 0; y < 2; ++y)
    {
        [unroll]
        for (int x = 0; x < 2; ++x)
        {
            float2 tapUv = (base + float2(x, y) + 0.5f) * invHalfSize;
            float bilinear = (x == 0 ? 1.0f - fraction.x : fraction.x) * (y == 0 ? 1.0f - fraction.y : fraction.y);
            float relativeDifference = abs(DistanceAt(tapUv) - centerDistance) / max(centerDistance, kMinDistance);
            float weight = bilinear / (kDepthEpsilon + relativeDifference);
            sum += gVolume.SampleLevel(gPointSampler, tapUv, 0).rgb * weight;
            weightSum += weight;
        }
    }
    return float4(sum / max(weightSum, kMinWeight), 0.0f);
}
