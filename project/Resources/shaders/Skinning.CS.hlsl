// Skinning Compute Shader
// スキニング計算を行うコンピュートシェーダー

// SkinnedVertexData のバイトオフセット (C++と完全一致)
// Vector4 position:    offset 0,  size 16
// Vector2 texcoord:    offset 16, size 8
// Vector3 normal:      offset 24, size 12
// uint32_t boneIndices[4]: offset 36, size 16
// float boneWeights[4]:    offset 52, size 16
// Total: 68 bytes

static const uint SKINNED_VERTEX_SIZE = 68;
static const uint OFFSET_POSITION = 0;
static const uint OFFSET_TEXCOORD = 16;
static const uint OFFSET_NORMAL = 24;
static const uint OFFSET_BONE_INDICES = 36;
static const uint OFFSET_BONE_WEIGHTS = 52;

// 出力頂点構造体
struct OutputVertex
{
    float4 position;
    float2 texcoord;
    float3 normal;
};

// ボーン行列バッファ
StructuredBuffer<float4x4> gBoneMatrices : register(t0);

// 入力頂点バッファ (ByteAddressBuffer で明示的にバイト読み込み)
ByteAddressBuffer gInputVertices : register(t1);

// 出力頂点バッファ（UAV）
RWStructuredBuffer<OutputVertex> gOutputVertices : register(u0);

// 頂点数
cbuffer SkinningConstants : register(b0)
{
    uint gVertexCount;
    uint3 gPadding;
};

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint vertexIndex = DTid.x;
    
    // 範囲外チェック
    if (vertexIndex >= gVertexCount)
    {
        return;
    }
    
    // この頂点のベースオフセット
    uint baseOffset = vertexIndex * SKINNED_VERTEX_SIZE;
    
    // 各フィールドを明示的なオフセットで読み込み
    float4 position = asfloat(gInputVertices.Load4(baseOffset + OFFSET_POSITION));
    float2 texcoord = asfloat(gInputVertices.Load2(baseOffset + OFFSET_TEXCOORD));
    float3 normal = asfloat(gInputVertices.Load3(baseOffset + OFFSET_NORMAL));
    uint4 boneIndices = gInputVertices.Load4(baseOffset + OFFSET_BONE_INDICES);
    float4 boneWeights = asfloat(gInputVertices.Load4(baseOffset + OFFSET_BONE_WEIGHTS));
    
    // スキニング行列の計算
    float4x4 skinMatrix = 
        gBoneMatrices[boneIndices.x] * boneWeights.x +
        gBoneMatrices[boneIndices.y] * boneWeights.y +
        gBoneMatrices[boneIndices.z] * boneWeights.z +
        gBoneMatrices[boneIndices.w] * boneWeights.w;
    
    // 位置を変換
    float4 skinnedPosition = mul(position, skinMatrix);
    
    // 法線を変換（3x3部分のみ使用）
    float3 transformedNormal = mul(normal, (float3x3)skinMatrix);
    float normalLen = length(transformedNormal);
    float3 skinnedNormal = (normalLen > 1e-5) ? transformedNormal / normalLen : normal;
    
    // 出力
    OutputVertex output;
    output.position = skinnedPosition;
    output.texcoord = texcoord;
    output.normal = -skinnedNormal; // 法線を反転してライティング修正
    
    gOutputVertices[vertexIndex] = output;
}
