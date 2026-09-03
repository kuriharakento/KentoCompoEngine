/**
 * ParticleCompute.hlsl
 * GPUパーティクルシミュレーション用 Compute Shader
 * 
 * C++側のParticle構造体と完全に同じレイアウト (128 bytes, 16-byte aligned)
 */

// パーティクル構造体（C++Particle構造体と完全一致 - 128 bytes）
struct Particle
{
    // Transform (48 bytes)
    float3 position;    // 12 bytes
    float pad0;         // 4 bytes
    
    float3 velocity;    // 12 bytes
    float pad1;         // 4 bytes
    
    float3 scale;       // 12 bytes
    float pad2;         // 4 bytes
    
    // Rotation (16 bytes)
    float4 rotation;    // Quaternion XYZW
    
    // Appearance (16 bytes)
    float4 color;       // RGBA
    
    // Initial Color (16 bytes)
    float4 initialColor; // InitialColorModuleで設定された初期カラー
    
    // Lifetime (16 bytes)
    float age;          // 経過時間
    float lifetime;     // 寿命
    float ribbonWidth;  // リボン幅
    uint flags;         // ビットフラグ
    
    // IDs (16 bytes)
    uint id;            // パーティクルID
    uint ribbonId;      // リボングループID
    uint spriteIndex;   // テクスチャシートフレーム
    uint pad3;          // アライメント
};

// 定数バッファ（C++側のGPUParticleConstantsと一致）
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
    float gpuEventVelocityScale; uint gpuEventInheritColor; float gpuSpawnLifetime; uint paddingGpuEvent;
    
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

    // Noise
    uint hasNoise;
    float noiseStrength;
    float noiseFrequency;
    float paddingNoise;

    // RotationOverLifetime
    uint hasRotationOL;
    float rotOLStartSpeed;
    float rotOLEndSpeed;
    uint rotOLEasing;

    // AlphaFade
    uint hasAlphaFade;
    float alphaFadeStart;
    float alphaFadeEnd;
    uint alphaFadeEaseIn;
    uint alphaFadeEaseOut;
    float3 paddingAlpha;

    // VelocityOverLifetime
    uint hasVelocityOL;
    float velocityOLStart;
    float velocityOLEnd;
    float paddingVelocityOL;

    // StretchByVelocity
    uint hasStretchByVelocity;
    float stretchFactor;
    float minStretch;
    float maxStretch;
    uint stretchPreserveVolume;
    float3 paddingStretch;

    // Flicker
    uint hasFlicker;
    float flickerFrequency;
    float flickerMinAlpha;
    float flickerMaxAlpha;
    uint flickerRandomPhase;
    uint flickerUseNoise;
    float2 paddingFlicker;

    // FaceVelocity
    uint hasFaceVelocity;
    uint faceVelocityUse2D;
    float2 paddingFaceVelocity;
	uint hasTextureSheet;
	uint textureSheetColumns;
	uint textureSheetRows;
	uint paddingTextureSheet;

    uint pureGpuEnabled;
    uint spawnCount;
    uint spawnSerialBase;
    uint spawnSeed;
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
};

// パーティクルバッファ (UAV)
RWStructuredBuffer<Particle> particles : register(u0);
RWByteAddressBuffer spawnCounter : register(u1);
struct EmitterState
{
    float rateAccumulator; float burstElapsed; uint burstLoop; uint burstFired;
    uint spawnCount; uint regularSpawnCount; uint eventSpawnCount; uint spawnSerialBase;
    uint totalSpawned; uint rateSpawnCount; uint2 padding;
};
RWStructuredBuffer<EmitterState> emitterState : register(u2);
struct ParticleEvent
{
    float3 position; uint type;
    float3 velocity; uint particleId;
    float4 color;
};
RWStructuredBuffer<ParticleEvent> particleEvents : register(u3);
RWByteAddressBuffer eventCounter : register(u4);
StructuredBuffer<ParticleEvent> sourceEvents : register(t0);
ByteAddressBuffer sourceEventCounter : register(t1);
ByteAddressBuffer moduleProgram : register(t2);
StructuredBuffer<float4> moduleLut : register(t3);

float4 SampleModuleLut(uint offset, uint count, float ratio)
{
    if (count == 0u) return 0.0f;
    float coordinate = saturate(ratio) * float(count - 1u);
    uint lower = (uint)floor(coordinate);
    uint upper = min(lower + 1u, count - 1u);
    return lerp(moduleLut[offset + lower], moduleLut[offset + upper], coordinate - float(lower));
}

// フラグ定数
static const uint FLAG_ALIVE = 1 << 0;

bool IsFiniteParticle(Particle p)
{
    return all(isfinite(p.position)) && all(isfinite(p.velocity)) &&
        all(isfinite(p.scale)) && all(isfinite(p.rotation)) &&
        all(isfinite(p.color)) && all(isfinite(p.initialColor)) &&
        isfinite(p.age) && isfinite(p.lifetime) && isfinite(p.ribbonWidth);
}
static const uint FLAG_RIBBON_HEAD = 1 << 1;

// イージング関数
float ApplyEasing(uint type, float t)
{
    if (type == 0) return t; // Linear
    if (type == 1) return 1.0f - cos(t * 1.5707963f); // EaseInSine
    if (type == 2) return sin(t * 1.5707963f); // EaseOutSine
    if (type == 3) return -(cos(3.1415926f * t) - 1.0f) * 0.5f; // EaseInOutSine
    if (type == 4) return t * t; // EaseInQuad
    if (type == 5) return t * (2.0f - t); // EaseOutQuad
    if (type == 6) return t < 0.5f ? 2.0f * t * t : 1.0f - pow(-2.0f * t + 2.0f, 2.0f) * 0.5f; // EaseInOutQuad
    return t;
}

// 決定論的乱数 (C++と同じハッシュ関数)
float DeterministicRandom(uint id, uint subSeed)
{
    uint x = id + subSeed * 0x9e3779b9u;
    x = ((x >> 16) ^ x) * 0x45d9f3bu;
    x = ((x >> 16) ^ x) * 0x45d9f3bu;
    x = (x >> 16) ^ x;
    return float(x) / 4294967295.0f;
}

/**
 * メインシミュレーションカーネル
 */
[numthreads(256, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint index = dispatchThreadId.x;
    
    // 範囲外チェック
    if (index >= particleCount)
        return;
    
    Particle p = particles[index];

    // Retire existing particles before dead-slot allocation. This permits a
    // slot that expires in this dispatch to be reused immediately instead of
    // introducing a whole-frame gap at low frame rates.
    if ((p.flags & FLAG_ALIVE) != 0u)
    {
        p.age += deltaTime;
        if (p.age >= p.lifetime)
        {
            uint deathEventIndex;
            eventCounter.InterlockedAdd(0, 1, deathEventIndex);
            if (deathEventIndex < maxParticles)
            {
                ParticleEvent deathEvent;
                deathEvent.position = p.position;
                deathEvent.type = 1;
                deathEvent.velocity = p.velocity;
                deathEvent.particleId = p.id;
                deathEvent.color = p.color;
                particleEvents[deathEventIndex] = deathEvent;
            }
            p.flags &= ~FLAG_ALIVE;
            particles[index] = p;
        }
    }
    
    // Pure GPU mode claims dead slots atomically. No particle snapshot is
    // read back to the CPU and no particle payload is uploaded per frame.
    if ((p.flags & FLAG_ALIVE) == 0)
    {
        if (pureGpuEnabled == 0 || emitterState[0].spawnCount == 0)
            return;

        uint claim;
        spawnCounter.InterlockedAdd(0, 1, claim);
        if (claim >= emitterState[0].spawnCount)
            return;

        uint serial = emitterState[0].spawnSerialBase + claim;
        float rx = DeterministicRandom(serial, spawnSeed + 1);
        float ry = DeterministicRandom(serial, spawnSeed + 2);
        float rz = DeterministicRandom(serial, spawnSeed + 3);
        float rl = DeterministicRandom(serial, spawnSeed + 4);
        float rsx = DeterministicRandom(serial, spawnSeed + 5);
        float rsy = DeterministicRandom(serial, spawnSeed + 6);
        float rsz = DeterministicRandom(serial, spawnSeed + 7);

        p.position = emitterPosition;
        p.velocity = lerp(initialVelocityMin, initialVelocityMax, float3(rx, ry, rz));

        if (hasSpawnShape != 0)
        {
            float3 offset = 0.0f;
            float3 direction = float3(0, 1, 0);
            float a = DeterministicRandom(serial, spawnSeed + 12);
            float b = DeterministicRandom(serial, spawnSeed + 13);
            float c = DeterministicRandom(serial, spawnSeed + 14);
            if (spawnShapeType == 1) // Sphere
            {
                float theta = a * 6.283185307f;
                float cosPhi = b * 2.0f - 1.0f;
                float sinPhi = sqrt(saturate(1.0f - cosPhi * cosPhi));
                float radius = spawnLocation == 1 ? spawnOuterRadius : lerp(spawnInnerRadius, spawnOuterRadius, c);
                direction = float3(sinPhi * cos(theta), cosPhi, sinPhi * sin(theta));
                offset = direction * radius;
            }
            else if (spawnShapeType == 2) // Circle
            {
                float angle = a * spawnArcRadians;
                float radius = (spawnLocation == 1 || spawnLocation == 2) ? spawnOuterRadius : lerp(spawnInnerRadius, spawnOuterRadius, b);
                direction = float3(cos(angle), 0, sin(angle));
                offset = direction * radius;
            }
            else if (spawnShapeType == 3) // Box
            {
                float3 halfSize = spawnBoxSize * 0.5f;
                offset = (float3(a, b, c) * 2.0f - 1.0f) * halfSize;
                if (spawnLocation == 1 || spawnLocation == 2)
                {
                    uint axis = serial % 3;
                    float signValue = DeterministicRandom(serial, spawnSeed + 15) < 0.5f ? -1.0f : 1.0f;
                    direction = 0.0f;
                    if (axis == 0) { offset.x = halfSize.x * signValue; direction.x = signValue; if (spawnLocation == 2) offset.y = halfSize.y * (b < 0.5f ? -1.0f : 1.0f); }
                    else if (axis == 1) { offset.y = halfSize.y * signValue; direction.y = signValue; if (spawnLocation == 2) offset.z = halfSize.z * (b < 0.5f ? -1.0f : 1.0f); }
                    else { offset.z = halfSize.z * signValue; direction.z = signValue; if (spawnLocation == 2) offset.x = halfSize.x * (b < 0.5f ? -1.0f : 1.0f); }
                }
            }
            else if (spawnShapeType == 4) // Cone
            {
                float angle = a * spawnArcRadians;
                float heightT = b;
                float radius = spawnOuterRadius * (1.0f - heightT);
                offset = float3(cos(angle) * radius, heightT * spawnConeHeight, sin(angle) * radius);
                float3 coneDirection = float3(cos(angle) * spawnConeHeight, spawnOuterRadius, sin(angle) * spawnConeHeight);
                float coneLengthSq = dot(coneDirection, coneDirection);
                direction = coneLengthSq > 0.000001f ? coneDirection * rsqrt(coneLengthSq) : float3(0, 1, 0);
            }
            else if (spawnShapeType == 5) // Line
            {
                offset = lerp(spawnLineStart, spawnLineEnd, a);
                direction = normalize(spawnLineEnd - spawnLineStart + float3(0, 0.00001f, 0));
            }
            p.position += offset;
            if (spawnEmitFromSurface != 0 && spawnInitialSpeed > 0.0f)
                p.velocity = direction * spawnInitialSpeed;
        }
        p.scale = lerp(initialScaleMin, initialScaleMax, float3(rsx, rsy, rsz));
        p.rotation = float4(0, 0, 0, 1);
        p.color = lerp(initialColorMin, initialColorMax,
            float4(DeterministicRandom(serial, spawnSeed + 8), DeterministicRandom(serial, spawnSeed + 9), DeterministicRandom(serial, spawnSeed + 10), DeterministicRandom(serial, spawnSeed + 11)));
        p.initialColor = p.color;
        p.lifetime = max(lerp(initialLifetimeMin, initialLifetimeMax, rl), 0.001f);
        // SpawnRate represents births distributed across this frame. Giving
        // each regular claim a sub-frame age keeps rate*lifetime stable even
        // when a frame contains a large spawn batch.
        p.age = claim < emitterState[0].rateSpawnCount && emitterState[0].rateSpawnCount > 0u
            ? min(deltaTime, max(p.lifetime, 0.001f)) * ((float(claim) + 0.5f) / float(emitterState[0].rateSpawnCount))
            : 0.0f;
        p.ribbonWidth = 1.0f;
        p.flags = FLAG_ALIVE;
        p.id = serial;
        p.ribbonId = gpuRibbonGroupCount > 1u ? serial % gpuRibbonGroupCount : 0u;
        p.spriteIndex = 0;

        if (hasGpuEventSource != 0 && claim >= emitterState[0].regularSpawnCount)
        {
            uint wanted = claim - emitterState[0].regularSpawnCount;
            uint sourceCount = min(sourceEventCounter.Load(0), maxParticles);
            [loop]
            for (uint sourceIndex = 0; sourceIndex < sourceCount; ++sourceIndex)
            {
                ParticleEvent sourceEvent = sourceEvents[sourceIndex];
                bool matches = sourceEvent.type == gpuEventTrigger &&
                    DeterministicRandom(sourceEvent.particleId, 17) <= gpuEventProbability;
                if (!matches) continue;
                if (wanted == 0)
                {
                    p.position = sourceEvent.position;
                    if (gpuEventInheritVelocity != 0) p.velocity = sourceEvent.velocity * gpuEventVelocityScale;
                    if (gpuEventInheritColor != 0) { p.color = sourceEvent.color; p.initialColor = sourceEvent.color; }
                    break;
                }
                wanted--;
            }
        }

        uint eventIndex;
        eventCounter.InterlockedAdd(0, 1, eventIndex);
        if (eventIndex < maxParticles)
        {
            ParticleEvent spawnEvent;
            spawnEvent.position = p.position;
            spawnEvent.type = 0;
            spawnEvent.velocity = p.velocity;
            spawnEvent.particleId = p.id;
            spawnEvent.color = p.color;
            particleEvents[eventIndex] = spawnEvent;
        }
    }
    
    // 寿命チェック
    if (p.age >= p.lifetime)
    {
        uint eventIndex;
        eventCounter.InterlockedAdd(0, 1, eventIndex);
        if (eventIndex < maxParticles)
        {
            ParticleEvent deathEvent;
            deathEvent.position = p.position;
            deathEvent.type = 1;
            deathEvent.velocity = p.velocity;
            deathEvent.particleId = p.id;
            deathEvent.color = p.color;
            particleEvents[eventIndex] = deathEvent;
        }
        p.flags &= ~FLAG_ALIVE;
        particles[index] = p;
        return;
    }
    
    float lifeRatio = saturate(p.age / p.lifetime);

    // Data-driven packed module program. Records are 64 bytes and may be
    // extended without changing the particle constant-buffer ABI.
    uint moduleCount = min(moduleProgram.Load(0), 255u);
    [loop]
    for (uint moduleIndex = 0; moduleIndex < moduleCount; ++moduleIndex)
    {
        uint baseAddress = 16u + moduleIndex * 64u;
        uint opcode = moduleProgram.Load(baseAddress);
        if (opcode == 1u) // Drag
        {
            float minimumDrag = asfloat(moduleProgram.Load(baseAddress + 4u));
            float maximumDrag = asfloat(moduleProgram.Load(baseAddress + 8u));
            float randomValue = DeterministicRandom(p.id, moduleIndex);
            p.velocity *= saturate(1.0f - lerp(minimumDrag, maximumDrag, randomValue) * deltaTime);
        }
        else if (opcode == 2u) // VelocityOverLifetime
        {
            float multiplier = lerp(asfloat(moduleProgram.Load(baseAddress + 4u)), asfloat(moduleProgram.Load(baseAddress + 8u)), lifeRatio);
            p.velocity *= 1.0f - (1.0f - multiplier) * deltaTime;
        }
        else if (opcode == 3u) // Noise
        {
            float strength = asfloat(moduleProgram.Load(baseAddress + 4u));
            float frequency = asfloat(moduleProgram.Load(baseAddress + 8u));
            float noiseTime = p.age * frequency;
            float particleOffset = float(p.id);
            p.velocity += float3(sin(noiseTime * 2.0f + particleOffset * 0.1f),
                sin(noiseTime * 2.3f + particleOffset * 0.2f),
                sin(noiseTime * 2.7f + particleOffset * 0.3f)) * strength * deltaTime;
        }
    }

    // 1. DragModule (空気抵抗)
    if (hasDrag != 0)
    {
        float r = DeterministicRandom(p.id, 0); // subSeed = 0
        float d = lerp(dragMin, dragMax, r);
        float factor = 1.0f - d * deltaTime;
        p.velocity *= saturate(factor);
    }

    // 1.2 VelocityOverLifetimeModule (寿命に応じた速度乗算)
    if (hasVelocityOL != 0)
    {
        float multiplier = lerp(velocityOLStart, velocityOLEnd, lifeRatio);
        float dampFactor = 1.0f - (1.0f - multiplier) * deltaTime;
        p.velocity *= dampFactor;
    }
    
    // 重力を適用
    p.velocity += gravity * deltaTime;

    // 4. NoiseModule (シンプルなサイン波ノイズ風の動き)
    if (hasNoise != 0)
    {
        float t = p.age * noiseFrequency;
        float idOffset = float(p.id);
        float3 noiseVal = float3(
            sin(t * 2.0f + idOffset * 0.1f) * noiseStrength,
            sin(t * 2.3f + idOffset * 0.2f) * noiseStrength,
            sin(t * 2.7f + idOffset * 0.3f) * noiseStrength
        );
        p.velocity += noiseVal * deltaTime;
    }
    
    // 位置を更新
    p.position += p.velocity * deltaTime;

    bool programHasColorFade = false;
    [loop]
    for (uint postModuleIndex = 0; postModuleIndex < moduleCount; ++postModuleIndex)
    {
        uint baseAddress = 16u + postModuleIndex * 64u;
        uint opcode = moduleProgram.Load(baseAddress);
        if (opcode == 4u) // ColorFade
        {
            programHasColorFade = true;
            uint useInitial = moduleProgram.Load(baseAddress + 4u);
            uint easing = moduleProgram.Load(baseAddress + 8u);
            float4 startColor = float4(asfloat(moduleProgram.Load(baseAddress + 12u)), asfloat(moduleProgram.Load(baseAddress + 16u)), asfloat(moduleProgram.Load(baseAddress + 20u)), asfloat(moduleProgram.Load(baseAddress + 24u)));
            float4 endColor = float4(asfloat(moduleProgram.Load(baseAddress + 28u)), asfloat(moduleProgram.Load(baseAddress + 32u)), asfloat(moduleProgram.Load(baseAddress + 36u)), asfloat(moduleProgram.Load(baseAddress + 40u)));
            uint hasGradient = moduleProgram.Load(baseAddress + 44u);
            p.color = hasGradient != 0u
                ? SampleModuleLut(moduleProgram.Load(baseAddress + 48u), moduleProgram.Load(baseAddress + 52u), lifeRatio)
                : lerp(useInitial != 0u ? p.initialColor : startColor, endColor, ApplyEasing(easing, lifeRatio));
        }
        else if (opcode == 5u) // ScaleOverLifetime
        {
            uint easing = moduleProgram.Load(baseAddress + 4u);
            float3 startScale = float3(asfloat(moduleProgram.Load(baseAddress + 8u)), asfloat(moduleProgram.Load(baseAddress + 12u)), asfloat(moduleProgram.Load(baseAddress + 16u)));
            float3 endScale = float3(asfloat(moduleProgram.Load(baseAddress + 20u)), asfloat(moduleProgram.Load(baseAddress + 24u)), asfloat(moduleProgram.Load(baseAddress + 28u)));
            float interpolation = moduleProgram.Load(baseAddress + 32u) != 0u
                ? SampleModuleLut(moduleProgram.Load(baseAddress + 36u), moduleProgram.Load(baseAddress + 40u), lifeRatio).x
                : ApplyEasing(easing, lifeRatio);
            p.scale = lerp(startScale, endScale, interpolation);
        }
        else if (opcode == 6u) // StretchByVelocity
        {
            float stretch = clamp(1.0f + length(p.velocity) * asfloat(moduleProgram.Load(baseAddress + 4u)),
                asfloat(moduleProgram.Load(baseAddress + 8u)), asfloat(moduleProgram.Load(baseAddress + 12u)));
            p.scale.y = stretch;
            if (moduleProgram.Load(baseAddress + 16u) != 0u) { float shrink = rsqrt(max(stretch, 0.0001f)); p.scale.x = shrink; p.scale.z = shrink; }
        }
        else if (opcode == 7u) // FaceVelocity
        {
            if (dot(p.velocity, p.velocity) > 0.0001f)
            {
                if (moduleProgram.Load(baseAddress + 4u) != 0u) p.rotation.z = atan2(p.velocity.y, p.velocity.x) - 1.57079633f;
                else { float3 direction = normalize(p.velocity); p.rotation.x = -atan2(direction.y, length(direction.xz)); p.rotation.y = atan2(direction.x, direction.z); p.rotation.z = 0.0f; }
            }
        }
        else if (opcode == 8u) // RotationOverLifetime
        {
            float speed = lerp(asfloat(moduleProgram.Load(baseAddress + 4u)), asfloat(moduleProgram.Load(baseAddress + 8u)), ApplyEasing(moduleProgram.Load(baseAddress + 12u), lifeRatio));
            p.rotation.z += speed * deltaTime * 0.01745329252f;
        }
        else if (opcode == 9u) // AlphaFade
        {
            float fadeT = lifeRatio;
            uint easeIn = moduleProgram.Load(baseAddress + 12u), easeOut = moduleProgram.Load(baseAddress + 16u);
            if (easeIn != 0u && easeOut != 0u) fadeT = fadeT * fadeT * (3.0f - 2.0f * fadeT);
            else if (easeIn != 0u) fadeT *= fadeT;
            else if (easeOut != 0u) fadeT = 1.0f - (1.0f - fadeT) * (1.0f - fadeT);
            p.color.a = lerp(asfloat(moduleProgram.Load(baseAddress + 4u)), asfloat(moduleProgram.Load(baseAddress + 8u)), fadeT);
        }
        else if (opcode == 10u) // Flicker
        {
            float flickerTime = p.age * asfloat(moduleProgram.Load(baseAddress + 4u));
            if (moduleProgram.Load(baseAddress + 16u) != 0u) flickerTime += float(p.id) * 0.1f;
            float value = moduleProgram.Load(baseAddress + 20u) != 0u
                ? (sin(flickerTime * 2.0f) + sin(flickerTime * 3.7f) + 2.0f) * 0.25f
                : (sin(flickerTime * 6.2831853f) + 1.0f) * 0.5f;
            p.color.a = lerp(asfloat(moduleProgram.Load(baseAddress + 8u)), asfloat(moduleProgram.Load(baseAddress + 12u)), value);
        }
    }
    
    // 2. ColorFadeModule
    if (hasColorFade != 0)
    {
        float t = ApplyEasing(colorFadeEasing, lifeRatio);
        float4 effectiveStart = (colorFadeUseInitial != 0) ? p.initialColor : colorFadeStart;
        p.color = lerp(effectiveStart, colorFadeEnd, t);
    }
    else if (!programHasColorFade)
    {
        // デフォルトのカラーフェード（寿命に応じてアルファを減少）
        p.color.a = saturate(1.0f - lifeRatio);
    }

    // 3. ScaleOverLifetimeModule
    if (hasScaleOL != 0)
    {
        float t = ApplyEasing(scaleOLEasing, lifeRatio);
        p.scale = lerp(scaleOLStart, scaleOLEnd, t);
    }

    // 3.5. StretchByVelocityModule (速度によるスケール伸長)
    if (hasStretchByVelocity != 0)
    {
        float speed = length(p.velocity);
        float stretch = 1.0f + speed * stretchFactor;
        stretch = clamp(stretch, minStretch, maxStretch);
        p.scale.y = stretch;
        if (stretchPreserveVolume != 0)
        {
            float shrink = 1.0f / sqrt(stretch);
            p.scale.x = shrink;
            p.scale.z = shrink;
        }
    }

    // 4.5. FaceVelocityModule (進行方向アライメント)
    if (hasFaceVelocity != 0)
    {
        float speedSq = dot(p.velocity, p.velocity);
        if (speedSq > 0.0001f)
        {
            if (faceVelocityUse2D != 0)
            {
                p.rotation.z = atan2(p.velocity.y, p.velocity.x) - (3.14159265f * 0.5f);
            }
            else
            {
                float3 normDirection = normalize(p.velocity);
                float yaw = atan2(normDirection.x, normDirection.z);
                float pitch = atan2(normDirection.y, sqrt(normDirection.x * normDirection.x + normDirection.z * normDirection.z));
                p.rotation.x = -pitch;
                p.rotation.y = yaw;
                p.rotation.z = 0.0f;
            }
        }
    }

    // 5. RotationOverLifetimeModule (回転速度のイージング変化と加算)
    if (hasRotationOL != 0)
    {
        float t = ApplyEasing(rotOLEasing, lifeRatio);
        float speed = lerp(rotOLStartSpeed, rotOLEndSpeed, t);
        // Z軸まわりの回転（ラジアンへ変換して加算）
        float angleRad = speed * deltaTime * (3.14159265f / 180.0f);
        p.rotation.z += angleRad;
    }

    // 6. AlphaFadeModule (アルファ値のみをシンプルにフェード)
    if (hasAlphaFade != 0)
    {
        float t = lifeRatio;
        if (alphaFadeEaseIn != 0 && alphaFadeEaseOut != 0)
        {
            t = t * t * (3.0f - 2.0f * t); // smoothstep
        }
        else if (alphaFadeEaseIn != 0)
        {
            t = t * t;
        }
        else if (alphaFadeEaseOut != 0)
        {
            t = 1.0f - (1.0f - t) * (1.0f - t);
        }
        p.color.a = lerp(alphaFadeStart, alphaFadeEnd, t);
    }

    // 6.5. FlickerModule (アルファ値の点滅)
    if (hasFlicker != 0)
    {
        float t = p.age * flickerFrequency;
        if (flickerRandomPhase != 0)
        {
            t += float(p.id) * 0.1f;
        }
        
        float alphaVal = 0.0f;
        if (flickerUseNoise != 0)
        {
            // ノイズベース
            alphaVal = (sin(t * 2.0f) + sin(t * 3.7f) + 2.0f) * 0.25f;
        }
        else
        {
            // シンプルなサイン波
            alphaVal = (sin(t * 3.14159265f * 2.0f) + 1.0f) * 0.5f;
        }
        p.color.a = flickerMinAlpha + (flickerMaxAlpha - flickerMinAlpha) * alphaVal;
    }
    
    // Never allow NaN/Inf payloads to reach SV_Position. Undefined rasterizer
    // input can produce a transient full-screen triangle on some drivers.
    if (!IsFiniteParticle(p))
    {
        p = (Particle)0;
        p.lifetime = 1.0f;
    }

    // 結果を書き戻し
    particles[index] = p;
}
