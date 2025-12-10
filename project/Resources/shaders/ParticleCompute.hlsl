// ParticleCompute.hlsl
// GPU Particle Simulation Compute Shader

// パーティクルデータ構造
struct Particle
{
    float3 position;
    float3 velocity;
    float3 scale;
    float4 rotation; // Quaternion
    float4 color;
    float age;
    float lifetime;
    float ribbonWidth;
    uint id;
    uint flags; // 0bit: alive
};

// 定数バッファ
cbuffer SimulationParams : register(b0)
{
    float deltaTime;
    float3 gravity;
    float drag;
    uint maxParticles;
    uint activeParticles;
    float3 emitterPosition;
    float pad0;
};

// パーティクルバッファ (Read/Write)
RWStructuredBuffer<Particle> particles : register(u0);

// 死亡パーティクルカウント (Atomic)
RWStructuredBuffer<uint> deadCount : register(u1);

// スレッドグループサイズ
[numthreads(256, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint index = dispatchThreadId.x;
    
    // 範囲外チェック
    if (index >= maxParticles)
        return;
    
    Particle p = particles[index];
    
    // 死亡済みはスキップ
    if ((p.flags & 1) == 0)
        return;
    
    // 寿命チェック
    if (p.age >= p.lifetime)
    {
        // 死亡フラグを立てる
        p.flags &= ~1u;
        particles[index] = p;
        
        // 死亡カウントをインクリメント
        uint dummy;
        InterlockedAdd(deadCount[0], 1, dummy);
        return;
    }
    
    // 物理シミュレーション
    // 重力適用
    p.velocity += gravity * deltaTime;
    
    // 抵抗適用
    p.velocity *= (1.0f - drag * deltaTime);
    
    // 位置更新
    p.position += p.velocity * deltaTime;
    
    // 経過時間更新
    p.age += deltaTime;
    
    // 寿命に応じたアルファフェード
    float normalizedAge = p.age / p.lifetime;
    p.color.a = 1.0f - normalizedAge;
    
    // 結果を書き戻し
    particles[index] = p;
}

// パーティクル生成用シェーダー
[numthreads(64, 1, 1)]
void CSSpawn(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint index = dispatchThreadId.x;
    
    if (index >= maxParticles)
        return;
    
    Particle p = particles[index];
    
    // 死亡パーティクルを再利用
    if ((p.flags & 1) == 0)
    {
        // 初期化（CPU側で設定される値を使用）
        p.position = emitterPosition;
        p.velocity = float3(0, 1, 0); // デフォルト上向き
        p.scale = float3(1, 1, 1);
        p.rotation = float4(0, 0, 0, 1);
        p.color = float4(1, 1, 1, 1);
        p.age = 0;
        p.lifetime = 1.0f;
        p.ribbonWidth = 1.0f;
        p.flags = 1; // alive
        
        particles[index] = p;
    }
}
