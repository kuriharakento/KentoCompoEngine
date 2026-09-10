// ライトビーム（コーンメッシュの加算合成）の共通定義

#ifndef BEAM_HLSLI
#define BEAM_HLSLI

// C++ 側の BeamRenderer::ConstantsForGPU と一致させること
cbuffer BeamConstants : register(b0)
{
    row_major float4x4 viewProjection;
    row_major float4x4 viewMatrix;
    row_major float4x4 invProjection;
    float3 apex;          // 円錐の頂点（光源の位置）
    float beamLength;     // 光源から底面までの長さ
    float3 axisX;         // 断面の軸
    float beamRadius;     // 底面の半径
    float3 axisY;         // 断面の軸
    float beamIntensity;  // 明るさ
    float3 beamDirection; // 光の向き
    float fadeDistance;   // 壁や床の手前で消える距離
    float3 beamColor;     // 光の色
    float edgePower;      // 縁の暗さ
    float2 screenSize;    // ビューの解像度
    float lengthFalloff;  // 光源から離れるほど暗くなる速さ
    float beamPadding;
};

// 法線と位置はビュー空間で持つ。カメラはビュー空間の原点にいるので、
// 視線ベクトルが「-位置」で求まり、カメラの位置を別に渡さずに済む。
struct BeamVertexOutput
{
    float4 position : SV_POSITION;
    float3 viewNormal : NORMAL0;
    float3 viewPosition : TEXCOORD0;
    float along : TEXCOORD1; // 光源から底面までの 0〜1
};

#endif // BEAM_HLSLI
