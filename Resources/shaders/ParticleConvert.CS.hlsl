#pragma pack_matrix(row_major)
#include "Particle.hlsli"

// シミュレーション用パーティクル構造体 (128 bytes - CPU Particle構造体と一致)
struct Particle
{
    float3 position; float pad0;
    float3 velocity; float pad1;
    float3 scale;    float pad2;
    float4 rotation;
    float4 color;
    float4 initialColor; // InitialColorModuleで設定された初期カラー
    float age; float lifetime; float ribbonWidth; uint flags;
    uint id; uint ribbonId; uint spriteIndex; uint pad3;
};

// カメラ情報
struct Camera
{
    float4x4 view;
    float4x4 projection;
    float3 eye;
    float pad;
};

// 入力：シミュレーション結果
StructuredBuffer<Particle> gParticles : register(t0);
// 出力：レンダリング用データ
RWStructuredBuffer<ParticleForGPU> gRenderParticles : register(u0);
RWByteAddressBuffer gDrawArguments : register(u1);

// 定数
cbuffer Constants : register(b0)
{
    float deltaTime;
    float totalTime;
    uint particleCount;
    uint maxParticles;
    float gpuRibbonWidth; uint gpuRibbonWidthFade; uint gpuRibbonAlphaFade; uint gpuRibbonGroupCount;
    float gpuSpawnRate; uint gpuBurstCount; float gpuBurstInterval; float gpuBurstDelay;
    int gpuBurstLoops; uint gpuEmitterIsEmitting; uint gpuEmitterReset; uint paddingEmitterState;
    uint hasGpuEventSource; uint gpuEventTrigger; float gpuEventProbability; uint gpuEventInheritVelocity;
    float gpuEventVelocityScale; uint gpuEventInheritColor; uint2 paddingGpuEvent;
    
    float3 emitterPosition;
    uint isBillboard;
    
    float3 gravity;
    uint simulationSpace;

    float4x4 emitterWorld;

	// 旧モジュール用の定数はモジュールプログラムに置き換えたので消した
    uint hasTextureSheet; uint textureSheetColumns; uint textureSheetRows; uint paddingTextureSheet;
    uint pureGpuEnabled; uint spawnCount; uint spawnSerialBase; uint spawnSeed;
    float3 initialVelocityMin; float initialLifetimeMin;
    float3 initialVelocityMax; float initialLifetimeMax;
    float3 initialScaleMin; float paddingPure0;
    float3 initialScaleMax; float paddingPure1;
    float4 initialColorMin;
    float4 initialColorMax;
    uint hasSpawnShape; uint spawnShapeType; uint spawnLocation; uint spawnEmitFromSurface;
    float spawnInnerRadius; float spawnOuterRadius; float spawnInitialSpeed; float spawnArcRadians;
    float3 spawnBoxSize; float spawnConeHeight;
    float3 spawnLineStart; float paddingShape0;
    float3 spawnLineEnd; float paddingShape1;
}

// カメラ定数
cbuffer CameraBuffer : register(b1)
{
    float4x4 cameraView;
    float4x4 cameraProjection;
    float3 cameraEye;
    float cameraPad;
}

// フラグ定数
static const uint FLAG_ALIVE = 1 << 0;

// X軸回転行列を生成
float4x4 MakeRotateXMatrix(float rad)
{
    float s = sin(rad);
    float c = cos(rad);
    float4x4 m = (float4x4)0;
    m[0][0] = 1.0f;
    m[1][1] = c;     m[1][2] = s;
    m[2][1] = -s;    m[2][2] = c;
    m[3][3] = 1.0f;
    return m;
}

// Y軸回転行列を生成
float4x4 MakeRotateYMatrix(float rad)
{
    float s = sin(rad);
    float c = cos(rad);
    float4x4 m = (float4x4)0;
    m[0][0] = c;     m[0][2] = -s;
    m[1][1] = 1.0f;
    m[2][0] = s;     m[2][2] = c;
    m[3][3] = 1.0f;
    return m;
}

// Z軸回転行列を生成
float4x4 MakeRotateZMatrix(float rad)
{
    float s = sin(rad);
    float c = cos(rad);
    float4x4 m = (float4x4)0;
    m[0][0] = c;     m[0][1] = s;
    m[1][0] = -s;    m[1][1] = c;
    m[2][2] = 1.0f;
    m[3][3] = 1.0f;
    return m;
}

// オイラー角 -> 回転行列 (X * Y * Z順)
float4x4 EulerToMatrix(float3 rot)
{
    float4x4 rx = MakeRotateXMatrix(rot.x);
    float4x4 ry = MakeRotateYMatrix(rot.y);
    float4x4 rz = MakeRotateZMatrix(rot.z);
    return mul(rx, mul(ry, rz));
}

// ビルボード回転行列生成
float4x4 CalculateBillboardMatrix(float3 position, float3 cameraPos)
{
    float3 toCamera = cameraPos - position;
    float distanceSq = dot(toCamera, toCamera);
    float3 forward = distanceSq > 0.000001f ? toCamera * rsqrt(distanceSq) : float3(0, 0, 1);
    float3 up = float3(0, 1, 0);
    // カメラが真上の場合
    if (abs(dot(forward, up)) > 0.99f) up = float3(0, 0, -1);
    
    float3 right = normalize(cross(up, forward));
    up = cross(forward, right);
    
    float4x4 m = (float4x4)0;
    m[0][0] = right.x;   m[0][1] = right.y;   m[0][2] = right.z;
    m[1][0] = up.x;      m[1][1] = up.y;      m[1][2] = up.z;
    m[2][0] = forward.x; m[2][1] = forward.y; m[2][2] = forward.z;
    m[3][3] = 1.0f;
    return m;
}

[numthreads(256, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint id = dtid.x;
    if (id >= particleCount) return;

    Particle p = gParticles[id];

    // Aliveチェック（Pure GPUでは描画数に数えないだけ。旧経路は透明インスタンスにする）
    if ((p.flags & FLAG_ALIVE) == 0)
    {
        if (pureGpuEnabled == 0)
        {
            gRenderParticles[id].World = (float4x4)0;
            gRenderParticles[id].color = float4(0,0,0,0);
        }
        return;
    }

    bool finitePayload = all(isfinite(p.position)) && all(isfinite(p.scale)) &&
        all(isfinite(p.rotation)) && all(isfinite(p.color));
    bool boundedPayload = all(abs(p.position) <= 1000000.0f) &&
        all(abs(p.scale) <= 10000.0f) && all(abs(p.rotation) <= 1000000.0f);
    if (!finitePayload || !boundedPayload)
    {
        if (pureGpuEnabled == 0)
        {
            gRenderParticles[id].World = (float4x4)0;
            gRenderParticles[id].color = float4(0,0,0,0);
        }
        return;
    }

    // スケール行列
    float4x4 scaleMat = (float4x4)0;
    scaleMat[0][0] = p.scale.x;
    scaleMat[1][1] = p.scale.y;
    scaleMat[2][2] = p.scale.z;
    scaleMat[3][3] = 1.0f;

    // 回転行列。ビルボードはCPUのスプライトと同じくZ回転だけを持ち、
    // カメラの回転は描画時に頂点シェーダーがビューごとに掛ける
    float4x4 rotMat = isBillboard ? MakeRotateZMatrix(p.rotation.z) : EulerToMatrix(p.rotation.xyz);
    float4x4 local = mul(scaleMat, rotMat);

    // World行列合成 (Scale * Rot * Trans)
    float4x4 world = local;
    world[3] = float4(p.position, 1.0f);

    // ローカルシミュレーションの場合はエミッターのワールド行列を適用
    if (simulationSpace == 1)
    {
        if (isBillboard)
        {
            // ビルボードは向きをカメラに任せるので、位置だけをワールドへ移す
            world[3] = float4(mul(float4(p.position, 1.0f), emitterWorld).xyz, 1.0f);
        }
        else
        {
            world = mul(world, emitterWorld);
        }
    }

    if (!all(isfinite(world[0])) || !all(isfinite(world[1])) || !all(isfinite(world[2])) || !all(isfinite(world[3])))
    {
        return;
    }

    uint outputIndex = id;
    if (pureGpuEnabled != 0)
    {
        gDrawArguments.InterlockedAdd(4, 1, outputIndex);
    }

    gRenderParticles[outputIndex].World = world;
    gRenderParticles[outputIndex].color = p.color;
    gRenderParticles[outputIndex].billboard = isBillboard != 0 ? 1u : 0u;
    gRenderParticles[outputIndex].padding = uint3(0, 0, 0);
	uint columns = max(textureSheetColumns, 1u);
	uint rows = max(textureSheetRows, 1u);
	uint frame = hasTextureSheet != 0 ? p.spriteIndex % (columns * rows) : 0u;
	float2 uvScale = hasTextureSheet != 0 ? rcp(float2(columns, rows)) : float2(1.0f, 1.0f);
	float2 uvOffset = float2(frame % columns, frame / columns) * uvScale;
	gRenderParticles[outputIndex].uvOffsetScale = float4(uvOffset, uvScale);
}
