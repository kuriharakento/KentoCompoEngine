// ライトビームの頂点シェーダー
// 単位円錐（頂点が原点、+Z に長さ1、底面の半径1）を、ライトの位置・向き・広がりに合わせて置く。
#include "Beam.hlsli"

struct BeamVertexInput
{
    float3 position : POSITION0;
};

BeamVertexOutput main(BeamVertexInput input)
{
    BeamVertexOutput output;

    float3 local = input.position;
    float3 world = apex
                 + axisX * (local.x * beamRadius)
                 + axisY * (local.y * beamRadius)
                 + beamDirection * (local.z * beamLength);

    // 拡大率が軸ごとに違う円錐の法線。
    // 面 (x/R)^2 + (y/R)^2 - (z/L)^2 = 0 の勾配を、ワールドの軸で組み立てる
    float3 normal = axisX * (local.x / beamRadius)
                  + axisY * (local.y / beamRadius)
                  - beamDirection * (local.z / beamLength);

    output.position = mul(float4(world, 1.0f), viewProjection);
    // カメラは回転と平行移動だけなので、ビュー行列の 3x3 部分で向きを変換できる
    output.viewNormal = normalize(mul(normal, (float3x3)viewMatrix));
    output.viewPosition = mul(float4(world, 1.0f), viewMatrix).xyz;
    output.along = local.z;
    return output;
}
