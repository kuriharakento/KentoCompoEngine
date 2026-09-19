#pragma pack_matrix(row_major)

struct Particle
{
    float3 position; float pad0;
    float3 velocity; float pad1;
    float3 scale; float pad2;
    float4 rotation;
    float4 color;
    float4 initialColor;
    float age; float lifetime; float ribbonWidth; uint flags;
    uint id; uint ribbonId; uint spriteIndex; uint pad3;
};

struct TrailVertex
{
    float3 position;
    float2 texcoord;
    float4 color;
};

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
};

cbuffer CameraConstants : register(b1)
{
    float4x4 view;
    float4x4 projection;
    float3 eye;
    float cameraPadding;
};

cbuffer SortConstants : register(b2)
{
    uint sortLevel;
    uint sortMask;
    uint sortCapacity;
};

StructuredBuffer<Particle> particles : register(t0);
RWStructuredBuffer<uint> prefix : register(u0);
RWStructuredBuffer<uint> groupCounts : register(u1);
RWStructuredBuffer<uint> groupOffsets : register(u2);
RWStructuredBuffer<TrailVertex> vertices : register(u3);
RWByteAddressBuffer drawArguments : register(u4);
// x=ribbonId, y=persistent particleId, z=pool slot, w=alive marker.
RWStructuredBuffer<uint4> sortEntries : register(u5);

groupshared uint scanValues[256];

[numthreads(256, 1, 1)]
void BuildPrefix(uint3 dispatchId : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    uint alive = dispatchId.x < particleCount && (particles[dispatchId.x].flags & 1u) != 0u ? 1u : 0u;
    scanValues[groupIndex] = alive;
    GroupMemoryBarrierWithGroupSync();
    [unroll]
    for (uint offset = 1; offset < 256; offset <<= 1)
    {
        uint value = groupIndex >= offset ? scanValues[groupIndex - offset] : 0u;
        GroupMemoryBarrierWithGroupSync();
        scanValues[groupIndex] += value;
        GroupMemoryBarrierWithGroupSync();
    }
    if (dispatchId.x < particleCount) prefix[dispatchId.x] = scanValues[groupIndex] - alive;
    if (groupIndex == 255u) groupCounts[groupId.x] = scanValues[255];
}

[numthreads(256, 1, 1)]
void ScanGroups(uint groupIndex : SV_GroupIndex)
{
    uint groupTotal = (particleCount + 255u) / 256u;
    uint value = groupIndex < groupTotal ? groupCounts[groupIndex] : 0u;
    scanValues[groupIndex] = value;
    GroupMemoryBarrierWithGroupSync();
    [unroll]
    for (uint offset = 1; offset < 256; offset <<= 1)
    {
        uint previous = groupIndex >= offset ? scanValues[groupIndex - offset] : 0u;
        GroupMemoryBarrierWithGroupSync();
        scanValues[groupIndex] += previous;
        GroupMemoryBarrierWithGroupSync();
    }
    if (groupIndex < groupTotal) groupOffsets[groupIndex] = scanValues[groupIndex] - value;
    if (groupIndex == 0u)
    {
        uint aliveCount = groupTotal == 0u ? 0u : scanValues[groupTotal - 1u];
        drawArguments.Store(0, aliveCount >= 2u ? (aliveCount - 1u) * 6u : 0u);
        drawArguments.Store(4, 1u);
        drawArguments.Store(8, 0u);
        drawArguments.Store(12, 0u);
    }
}

[numthreads(256, 1, 1)]
void InitializeSort(uint3 dispatchId : SV_DispatchThreadID)
{
    uint index = dispatchId.x;
    if (index >= sortCapacity) return;
    if (index < particleCount && (particles[index].flags & 1u) != 0u)
    {
        Particle particle = particles[index];
        sortEntries[index] = uint4(particle.ribbonId, particle.id, index, 1u);
    }
    else
    {
        sortEntries[index] = uint4(0xffffffffu, 0xffffffffu, index, 0u);
    }
}

bool KeyGreater(uint4 lhs, uint4 rhs)
{
    return lhs.x > rhs.x || (lhs.x == rhs.x && lhs.y > rhs.y);
}

[numthreads(256, 1, 1)]
void BitonicSort(uint3 dispatchId : SV_DispatchThreadID)
{
    uint index = dispatchId.x;
    if (index >= sortCapacity) return;
    uint partner = index ^ sortMask;
    if (partner <= index) return;
    uint4 left = sortEntries[index];
    uint4 right = sortEntries[partner];
    bool ascending = (index & sortLevel) == 0u;
    bool swapValues = ascending ? KeyGreater(left, right) : KeyGreater(right, left);
    if (swapValues)
    {
        sortEntries[index] = right;
        sortEntries[partner] = left;
    }
}

float3 SafeNormalize(float3 value, float3 fallback)
{
    float lengthSquared = dot(value, value);
    return lengthSquared > 1.0e-8f ? value * rsqrt(lengthSquared) : fallback;
}

void BuildPair(Particle particle, float3 tangent, float v, out TrailVertex leftVertex, out TrailVertex rightVertex)
{
    float3 toCamera = SafeNormalize(eye - particle.position, float3(0.0f, 1.0f, 0.0f));
    float3 fallback = abs(tangent.y) > 0.9f ? float3(1.0f, 0.0f, 0.0f)
        : SafeNormalize(cross(tangent, float3(0.0f, 1.0f, 0.0f)), float3(1.0f, 0.0f, 0.0f));
    float3 right = SafeNormalize(cross(tangent, toCamera), fallback);
    float ageRatio = particle.lifetime > 0.0f ? saturate(particle.age / particle.lifetime) : 1.0f;
    float width = gpuRibbonWidth * particle.scale.x * (gpuRibbonWidthFade != 0u ? 1.0f - ageRatio : 1.0f);
    float4 color = particle.color;
    if (gpuRibbonAlphaFade != 0u) color.a *= 1.0f - ageRatio;
    float halfWidth = width * 0.5f;
    leftVertex.position = particle.position - right * halfWidth;
    leftVertex.texcoord = float2(0.0f, v);
    leftVertex.color = color;
    rightVertex.position = particle.position + right * halfWidth;
    rightVertex.texcoord = float2(1.0f, v);
    rightVertex.color = color;
}

[numthreads(256, 1, 1)]
void EmitVertices(uint3 dispatchId : SV_DispatchThreadID)
{
    uint segment = dispatchId.x;
    uint aliveCount = drawArguments.Load(0) / 6u + (drawArguments.Load(0) > 0u ? 1u : 0u);
    if (segment + 1u >= aliveCount) return;

    uint4 firstEntry = sortEntries[segment];
    uint4 secondEntry = sortEntries[segment + 1u];
    Particle first = particles[firstEntry.z];
    Particle second = particles[secondEntry.z];
    float3 tangent = SafeNormalize(second.position - first.position,
        SafeNormalize(first.velocity, float3(0.0f, 0.0f, 1.0f)));
    float denominator = max(1.0f, (float)(aliveCount - 1u));
    TrailVertex firstLeft, firstRight, secondLeft, secondRight;
    BuildPair(first, tangent, (float)segment / denominator, firstLeft, firstRight);
    BuildPair(second, tangent, (float)(segment + 1u) / denominator, secondLeft, secondRight);

    uint output = segment * 6u;
    if (firstEntry.x == secondEntry.x)
    {
        // Four useful strip vertices followed by two duplicates. Together with
        // the next block these duplicates restart the strip without a draw split.
        vertices[output + 0u] = firstLeft;
        vertices[output + 1u] = firstRight;
        vertices[output + 2u] = secondLeft;
        vertices[output + 3u] = secondRight;
        vertices[output + 4u] = secondRight;
        vertices[output + 5u] = secondRight;
    }
    else
    {
        // Ribbon boundary: an entirely degenerate block prevents cross-group triangles.
        firstLeft.color.a = 0.0f;
        [unroll] for (uint i = 0u; i < 6u; ++i) vertices[output + i] = firstLeft;
    }
}
