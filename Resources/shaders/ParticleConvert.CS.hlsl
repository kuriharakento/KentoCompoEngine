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

    // 追加モジュールパラメータ (アプローチB)
    uint hasDrag;
    float dragMin;
    float dragMax;
    float paddingDrag;
    
    uint hasColorFade;
    uint colorFadeUseInitial;
    uint colorFadeEasing;
    float paddingCF;
    float4 colorFadeStart;
    float4 colorFadeEnd;
    
    uint hasScaleOL;
    uint scaleOLEasing;
    float2 paddingScaleOL;
    float3 scaleOLStart;
    float paddingS1;
    float3 scaleOLEnd;
    float paddingS2;

    uint hasNoise; float noiseStrength; float noiseFrequency; float paddingNoise;
    uint hasRotationOL; float rotOLStartSpeed; float rotOLEndSpeed; uint rotOLEasing;
    uint hasAlphaFade; float alphaFadeStart; float alphaFadeEnd; uint alphaFadeEaseIn;
    uint alphaFadeEaseOut; float3 paddingAlpha;
    uint hasVelocityOL; float velocityOLStart; float velocityOLEnd; float paddingVelocityOL;
    uint hasStretchByVelocity; float stretchFactor; float minStretch; float maxStretch;
    uint stretchPreserveVolume; float3 paddingStretch;
    uint hasFlicker; float flickerFrequency; float flickerMinAlpha; float flickerMaxAlpha;
    uint flickerRandomPhase; uint flickerUseNoise; float2 paddingFlicker;
    uint hasFaceVelocity; uint faceVelocityUse2D; float2 paddingFaceVelocity;
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
    
    // Aliveチェック
    if ((p.flags & FLAG_ALIVE) == 0)
    {
        gRenderParticles[id].World = (float4x4)0;
        gRenderParticles[id].WVP = (float4x4)0;
        gRenderParticles[id].color = float4(0,0,0,0);
        return;
    }

    bool finitePayload = all(isfinite(p.position)) && all(isfinite(p.scale)) &&
        all(isfinite(p.rotation)) && all(isfinite(p.color));
    bool boundedPayload = all(abs(p.position) <= 1000000.0f) &&
        all(abs(p.scale) <= 10000.0f) && all(abs(p.rotation) <= 1000000.0f);
    if (!finitePayload || !boundedPayload)
    {
        gRenderParticles[id].World = (float4x4)0;
        gRenderParticles[id].WVP = (float4x4)0;
        gRenderParticles[id].color = float4(0,0,0,0);
        return;
    }

    // スケール行列
    float4x4 scaleMat = (float4x4)0;
    scaleMat[0][0] = p.scale.x;
    scaleMat[1][1] = p.scale.y;
    scaleMat[2][2] = p.scale.z;
    scaleMat[3][3] = 1.0f;

    // 回転行列
    float4x4 rotMat;
    if (isBillboard)
    {
        rotMat = CalculateBillboardMatrix(p.position, cameraEye);
        // Z回転のみ適用する？（通常ビルボードはZ回転許容）
        // ここでは簡易ビルボードとする
    }
    else
    {
        rotMat = EulerToMatrix(p.rotation.xyz);
    }

    // 平行移動行列
    float4x4 transMat = (float4x4)0;
    transMat[0][0] = 1.0f; transMat[1][1] = 1.0f; transMat[2][2] = 1.0f; transMat[3][3] = 1.0f;
    transMat[3][0] = p.position.x;
    transMat[3][1] = p.position.y;
    transMat[3][2] = p.position.z;

    // World行列合成 (Scale * Rot * Trans)
    float4x4 world = mul(scaleMat, mul(rotMat, transMat));

    // ローカルシミュレーションの場合はエミッターのワールド行列を適用
    if (simulationSpace == 1)
    {
        world = mul(world, emitterWorld);
    }

    // WVP行列
    float4x4 viewProj = mul(cameraView, cameraProjection);
    float4x4 wvp = mul(world, viewProj);
	float4 clipCenter = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), wvp);

    // Reject invalid clip transforms before incrementing the indirect instance
    // count. NaN/Inf in SV_Position has driver-dependent rasterization results.
    if (!all(isfinite(world[0])) || !all(isfinite(world[1])) || !all(isfinite(world[2])) || !all(isfinite(world[3])) ||
        !all(isfinite(wvp[0])) || !all(isfinite(wvp[1])) || !all(isfinite(wvp[2])) || !all(isfinite(wvp[3])) ||
		!all(isfinite(clipCenter)) || clipCenter.w <= 0.0001f)
    {
        return;
    }

    uint outputIndex = id;
    if (pureGpuEnabled != 0)
    {
        gDrawArguments.InterlockedAdd(4, 1, outputIndex);
    }

    gRenderParticles[outputIndex].World = world;
    gRenderParticles[outputIndex].WVP = wvp;
    gRenderParticles[outputIndex].color = p.color;
	uint columns = max(textureSheetColumns, 1u);
	uint rows = max(textureSheetRows, 1u);
	uint frame = hasTextureSheet != 0 ? p.spriteIndex % (columns * rows) : 0u;
	float2 uvScale = hasTextureSheet != 0 ? rcp(float2(columns, rows)) : float2(1.0f, 1.0f);
	float2 uvOffset = float2(frame % columns, frame / columns) * uvScale;
	gRenderParticles[outputIndex].uvOffsetScale = float4(uvOffset, uvScale);
}
