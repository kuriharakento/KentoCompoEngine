#pragma pack_matrix(row_major)
#include "Particle.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
};

// ParticleForGPUはParticle.hlsliで定義済み



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
    float4x4 wvp = gParticle[instanceId].WVP;

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
    output.color = validQuad ? gParticle[instanceId].color : 0.0f;
    return output;
}
