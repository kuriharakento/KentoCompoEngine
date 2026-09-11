// ライトビームのピクセルシェーダー
// 加算合成で描く。次の3つを掛け合わせて「空気中の光の筋」に見せる。
//   1. 光源から離れるほど暗い
//   2. 円錐の縁ほど暗い（筋の中心が明るく見える）
//   3. 壁や床に近いほど暗い（深度フェード）
#include "Beam.hlsli"

// これより短い法線は向きが決まらないとみなす（長さの2乗）
static const float kMinNormalLengthSq = 1.0e-12f;

Texture2D<float> gDepth : register(t0);
SamplerState gPointSampler : register(s0);

// シーンの深度からビュー空間の深度を求める
float SceneViewDepth(float2 uv)
{
    float depth = gDepth.Sample(gPointSampler, uv);
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 view = mul(float4(ndc, depth, 1.0f), invProjection);
    return view.z / view.w;
}

float4 main(BeamVertexOutput input) : SV_TARGET
{
    // 1. 光源から離れるほど暗くする。頂点付近は明るすぎないよう少しだけ抑える
    float along = saturate(input.along);
    float lengthFade = pow(1.0f - along, lengthFalloff) * smoothstep(0.0f, 0.05f, along);

    // 2. 円錐の縁ほど暗くする。視線と面が向き合う（筋の中心）ほど明るい。
    //    ビュー空間ではカメラが原点にいるので、視線は「-位置」
    float3 toEye = normalize(-input.viewPosition);
    // 補間された法線がほぼゼロのときも NaN にしない（NaN は加算しても画面を黒く塗る）
    float3 viewNormal = (dot(input.viewNormal, input.viewNormal) > kMinNormalLengthSq) ? normalize(input.viewNormal) : toEye;
    float facing = abs(dot(viewNormal, toEye));
    float edgeFade = pow(facing, edgePower);

    // 3. 壁や床の手前で消す。後ろに回った部分（差が負）は描かない
    float2 uv = input.position.xy / screenSize;
    float sceneDepth = SceneViewDepth(uv);
    float depthFade = saturate((sceneDepth - input.viewPosition.z) / max(fadeDistance, 0.001f));

    float3 light = beamColor * beamIntensity * lengthFade * edgeFade * depthFade;
    return float4(light, 0.0f);
}
