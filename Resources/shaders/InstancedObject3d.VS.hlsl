#include "Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
    // 1体ごとの色（マテリアルの色に掛ける）。C++ の InstanceData と同じ並び
    float4 color;
};

// インスタンスごとの行列バッファ
StructuredBuffer<TransformationMatrix> gInstanceMatrices : register(t0);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

// 頂点シェーダーの出力。Object3d.hlsli の VertexShaderOutput に、1体ごとの色を足した形（InstancedObject3d.PS と同じにする）
struct InstancedVertexOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPos : POSITION0;
    float32_t4 instanceColor : COLOR0;
};

InstancedVertexOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    InstancedVertexOutput output;
    
    // インスタンスIDを使用して行列を取得
    float4x4 worldMatrix = gInstanceMatrices[instanceId].World;
    float4x4 wvpMatrix = gInstanceMatrices[instanceId].WVP;
    float4x4 worldInverseTranspose = gInstanceMatrices[instanceId].WorldInverseTranspose;

    output.position = mul(input.position, wvpMatrix);
    output.worldPos = mul(input.position, worldMatrix).xyz;
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3)worldInverseTranspose));
    output.instanceColor = gInstanceMatrices[instanceId].color;
    
    return output;
}
