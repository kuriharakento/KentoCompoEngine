// GPU-resident emitter control state. One thread computes this frame's spawn
// request before the particle pool dispatch; no particle or emitter state is
// read back to the CPU.
cbuffer Constants : register(b0)
{
    float deltaTime;
    float totalTime;
    uint particleCount;
    uint maxParticles;
    float gpuRibbonWidth;
    uint gpuRibbonWidthFade;
    uint gpuRibbonAlphaFade;
    uint gpuRibbonGroupCount;
    float gpuSpawnRate;
    uint gpuBurstCount;
    float gpuBurstInterval;
    float gpuBurstDelay;
    int gpuBurstLoops;
    uint gpuEmitterIsEmitting;
    uint gpuEmitterReset;
    uint paddingEmitterState;
    uint hasGpuEventSource;
    uint gpuEventTrigger;
    float gpuEventProbability;
    uint gpuEventInheritVelocity;
    float gpuEventVelocityScale;
    uint gpuEventInheritColor;
    float gpuSpawnLifetime;
    uint paddingGpuEvent;
};

struct EmitterState
{
    float rateAccumulator;
    float burstElapsed;
    uint burstLoop;
    uint burstFired;
    uint spawnCount;
    uint regularSpawnCount;
    uint eventSpawnCount;
    uint spawnSerialBase;
    uint totalSpawned;
    uint rateSpawnCount;
    uint2 padding;
};

RWStructuredBuffer<EmitterState> emitterState : register(u2);
struct ParticleEvent { float3 position; uint type; float3 velocity; uint particleId; float4 color; };
StructuredBuffer<ParticleEvent> sourceEvents : register(t0);
ByteAddressBuffer sourceEventCounter : register(t1);

float EventRandom(uint id)
{
    uint x = id + 17u * 0x9e3779b9u;
    x = ((x >> 16) ^ x) * 0x45d9f3bu;
    x = ((x >> 16) ^ x) * 0x45d9f3bu;
    x = (x >> 16) ^ x;
    return (float)x / 4294967295.0f;
}

[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    EmitterState state = emitterState[0];
    if (gpuEmitterReset != 0)
    {
        state = (EmitterState)0;
    }
    state.spawnCount = 0;
    state.regularSpawnCount = 0;
    state.eventSpawnCount = 0;
	state.rateSpawnCount = 0;
	state.spawnSerialBase = state.totalSpawned;

    if (gpuEmitterIsEmitting != 0)
    {
        state.rateAccumulator += max(gpuSpawnRate, 0.0f) * deltaTime;
        uint rateCount = (uint)state.rateAccumulator;
        state.rateAccumulator -= (float)rateCount;

        uint burstCount = 0;
        if (gpuBurstCount > 0)
        {
            state.burstElapsed += deltaTime;
            bool loopsRemain = gpuBurstLoops < 0 || state.burstLoop < (uint)gpuBurstLoops;
            bool delayReady = state.burstFired != 0 || state.burstElapsed >= gpuBurstDelay;
            bool intervalReady = state.burstFired == 0 ||
                (gpuBurstInterval > 0.0f && state.burstElapsed >= gpuBurstInterval);
            if (loopsRemain && delayReady && intervalReady)
            {
                burstCount = gpuBurstCount;
                state.burstElapsed = 0.0f;
                state.burstFired = 1;
                state.burstLoop++;
            }
        }

		uint retainedRateCount = min(rateCount, (uint)ceil(max(gpuSpawnRate, 0.0f) * max(gpuSpawnLifetime, 0.001f)) + 1u);
		uint droppedRateCount = rateCount - retainedRateCount;
		state.rateSpawnCount = min(retainedRateCount, maxParticles);
		state.regularSpawnCount = min(state.rateSpawnCount + burstCount, maxParticles);
		state.spawnSerialBase = state.totalSpawned + droppedRateCount;
        if (hasGpuEventSource != 0)
        {
            uint sourceCount = min(sourceEventCounter.Load(0), maxParticles);
            [loop]
            for (uint i = 0; i < sourceCount; ++i)
            {
                ParticleEvent evt = sourceEvents[i];
                if (evt.type == gpuEventTrigger && EventRandom(evt.particleId) <= gpuEventProbability)
                    state.eventSpawnCount++;
            }
        }
        state.eventSpawnCount = min(state.eventSpawnCount, maxParticles - state.regularSpawnCount);
        state.spawnCount = state.regularSpawnCount + state.eventSpawnCount;
		state.totalSpawned += rateCount + burstCount + state.eventSpawnCount;
    }

    emitterState[0] = state;
}
