struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

// パーティクルのインスタンスデータ（C++側 ParticleGPU と一致）
// カメラに依存する値は持たず、描画時のビュー定数で頂点シェーダーが合成する
struct ParticleForGPU
{
    float4x4 World;       // billboard != 0 のとき上3x3はビルボード前のローカル回転・スケール
    float4 color;
    float4 uvOffsetScale; // テクスチャシート用 (offsetX, offsetY, scaleX, scaleY)
    uint billboard;
    uint3 padding;
};
