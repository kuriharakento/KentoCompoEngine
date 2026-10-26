#pragma pack_matrix(row_major)
#include "Particle.hlsli"

// 描画するカメラのビュー定数（ビューごとにCPUで確保される）
cbuffer ParticleView : register(b0)
{
    float4x4 gViewProjection;
    float4x4 gBillboard; // カメラの回転（平行移動なし）
};

StructuredBuffer<ParticleForGPU> gParticle : register(t0);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texxcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    ParticleForGPU particle = gParticle[instanceId];
    float4x4 world = particle.World;

    // ビルボード: ローカルの回転・スケールの後にカメラの回転を掛ける（平行移動はそのまま）
    if (particle.billboard != 0)
    {
        float3x3 oriented = mul((float3x3)world, (float3x3)gBillboard);
        world[0].xyz = oriented[0];
        world[1].xyz = oriented[1];
        world[2].xyz = oriented[2];
    }
    float4x4 wvp = mul(world, gViewProjection);

    // Reject the whole billboard when one of its corners crosses the camera
    // plane. Rejecting only the offending vertex lets the remaining triangle
    // undergo a near-plane perspective explosion and can cover the screen for
    // a frame. Every vertex of the instance evaluates the same four corners,
    // so the quad is discarded consistently without a CPU readback.
    float4 corner0 = mul(float4(-1.0f, -1.0f, 0.0f, 1.0f), wvp);
    float4 corner1 = mul(float4( 1.0f, -1.0f, 0.0f, 1.0f), wvp);
    float4 corner2 = mul(float4(-1.0f,  1.0f, 0.0f, 1.0f), wvp);
    float4 corner3 = mul(float4( 1.0f,  1.0f, 0.0f, 1.0f), wvp);
    bool validQuad = all(isfinite(corner0)) && all(isfinite(corner1)) &&
        all(isfinite(corner2)) && all(isfinite(corner3)) &&
        min(min(corner0.w, corner1.w), min(corner2.w, corner3.w)) > 0.001f;

    output.position = validQuad ? mul(input.position, wvp) : float4(2.0f, 2.0f, 0.0f, 1.0f);
    output.texcoord = input.texxcoord;
    output.color = validQuad ? particle.color : 0.0f;
    return output;
}
