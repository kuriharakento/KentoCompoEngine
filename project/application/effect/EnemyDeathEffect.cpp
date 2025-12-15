#include "EnemyDeathEffect.h"
#include "effects/particle/ParticleManager.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/update/UpdateModules.h"

namespace
{
// 血飛沫エフェクト設定
constexpr int kBloodBurstCount = 15;
constexpr float kBloodBurstDuration = 0.05f;

// 破片エフェクト設定
constexpr int kFragmentBurstCount = 8;
constexpr float kFragmentBurstDuration = 0.1f;

// 爆発エフェクト設定
constexpr int kExplosionBurstCount = 20;
constexpr float kExplosionBurstDuration = 0.05f;

// 電撃エフェクト設定
constexpr int kElectricBurstCount = 12;
constexpr float kElectricBurstDuration = 0.03f;

// 溶解エフェクト設定
constexpr float kDissolveSpawnRate = 30.0f;

// 煙エフェクト設定
constexpr int kSmokeBurstCount = 5;
constexpr float kSmokeBurstDuration = 0.1f;
}

EnemyDeathEffect::EnemyDeathEffect() = default;
EnemyDeathEffect::~EnemyDeathEffect() = default;

void EnemyDeathEffect::Initialize()
{
// 各エミッターを初期化
InitializeBloodEmitter();
InitializeFragmentEmitter();
InitializeExplosionEmitter();
InitializeElectricEmitter();
InitializeDissolveEmitter();
InitializeSmokeEmitter();
}

void EnemyDeathEffect::InitializeBloodEmitter()
{
    // 血飛沫エミッターの作成と初期化
    auto emitter = std::make_unique<ParticleEmitter>();
    emitter->Initialize("enemy_blood");
    
    // レンダラー設定（アルファブレンドで赤い血を表現）
    
    auto renderer = std::make_unique<SpriteRenderer>();
    renderer->Initialize(bloodTexturePath_);
    renderer->SetBlendMode(BlendMode::Alpha);
    emitter->SetRenderer(std::move(renderer));
    
    // バースト生成とパラメータ設定（上向きに飛散して重力で落ちる）
    emitter->AddModule(std::make_unique<SpawnBurstModule>(kBloodBurstCount, kBloodBurstDuration));
    emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.3f, -0.3f, -0.3f), Vector3(0.3f, 0.3f, 0.3f)));
    emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-3.0f, 1.0f, -3.0f), Vector3(3.0f, 5.0f, 3.0f)));
    emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.3f, 0.6f));
    emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(0.1f, 0.1f, 0.1f), Vector3(0.3f, 0.3f, 0.3f)));
    emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.8f, 0.1f, 0.1f, 1.0f)));
    emitter->AddModule(std::make_unique<GravityModule>(Vector3(0.0f, -15.0f, 0.0f)));
    emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(0.8f, 0.1f, 0.1f, 1.0f), Vector4(0.5f, 0.0f, 0.0f, 0.0f)));
    
    ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
}

void EnemyDeathEffect::InitializeFragmentEmitter()
{
    auto emitter = std::make_unique<ParticleEmitter>();
    emitter->Initialize("enemy_fragment");
    
    auto renderer = std::make_unique<SpriteRenderer>();
    renderer->Initialize(fragmentTexturePath_);
    renderer->SetBlendMode(BlendMode::Alpha);
    emitter->SetRenderer(std::move(renderer));
    
    emitter->AddModule(std::make_unique<SpawnBurstModule>(8, 0.1f));
    emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.2f, -0.2f, -0.2f), Vector3(0.2f, 0.2f, 0.2f)));
    emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-5.0f, 2.0f, -5.0f), Vector3(5.0f, 8.0f, 5.0f)));
    emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.5f, 1.0f));
    emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(0.05f, 0.05f, 0.05f), Vector3(0.15f, 0.15f, 0.15f)));
    emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.4f, 0.4f, 0.4f, 1.0f)));
    emitter->AddModule(std::make_unique<GravityModule>(Vector3(0.0f, -12.0f, 0.0f)));
    emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(0.4f, 0.4f, 0.4f, 1.0f), Vector4(0.2f, 0.2f, 0.2f, 0.0f)));
    
    ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
}

void EnemyDeathEffect::InitializeExplosionEmitter()
{
    auto emitter = std::make_unique<ParticleEmitter>();
    emitter->Initialize("enemy_explosion");
    
    auto renderer = std::make_unique<SpriteRenderer>();
    renderer->Initialize(explosionTexturePath_);
    renderer->SetBlendMode(BlendMode::Additive);
    emitter->SetRenderer(std::move(renderer));
    
    emitter->AddModule(std::make_unique<SpawnBurstModule>(20, 0.05f));
    emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.5f, -0.5f, -0.5f), Vector3(0.5f, 0.5f, 0.5f)));
    emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-6.0f, -6.0f, -6.0f), Vector3(6.0f, 6.0f, 6.0f)));
    emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.3f, 0.5f));
    emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(0.2f, 0.2f, 0.2f), Vector3(0.5f, 0.5f, 0.5f)));
    emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(1.0f, 0.5f, 0.0f, 1.0f)));
    emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(1.0f, 0.5f, 0.0f, 1.0f), Vector4(1.0f, 0.0f, 0.0f, 0.0f)));
    emitter->AddModule(std::make_unique<ScaleOverLifetimeModule>(Vector3(0.5f, 0.5f, 0.5f), Vector3(0.0f, 0.0f, 0.0f)));
    
    ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
}

void EnemyDeathEffect::InitializeElectricEmitter()
{
    auto emitter = std::make_unique<ParticleEmitter>();
    emitter->Initialize("enemy_electric");
    
    auto renderer = std::make_unique<SpriteRenderer>();
    renderer->Initialize(electricTexturePath_);
    renderer->SetBlendMode(BlendMode::Additive);
    emitter->SetRenderer(std::move(renderer));
    
    emitter->AddModule(std::make_unique<SpawnBurstModule>(12, 0.03f));
    emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.8f, -0.8f, -0.8f), Vector3(0.8f, 0.8f, 0.8f)));
    emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-8.0f, -8.0f, -8.0f), Vector3(8.0f, 8.0f, 8.0f)));
    emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.1f, 0.3f));
    emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(0.05f, 0.05f, 0.05f), Vector3(0.2f, 0.2f, 0.2f)));
    emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.5f, 0.7f, 1.0f, 1.0f)));
    emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(0.5f, 0.7f, 1.0f, 1.0f), Vector4(1.0f, 1.0f, 1.0f, 0.0f)));
    
    ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
}

void EnemyDeathEffect::InitializeDissolveEmitter()
{
    auto emitter = std::make_unique<ParticleEmitter>();
    emitter->Initialize("enemy_dissolve");
    
    auto renderer = std::make_unique<SpriteRenderer>();
    renderer->Initialize(dissolveTexturePath_);
    renderer->SetBlendMode(BlendMode::Additive);
    emitter->SetRenderer(std::move(renderer));
    
    emitter->AddModule(std::make_unique<SpawnRateModule>(30.0f));
    emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.5f, 0.0f, -0.5f), Vector3(0.5f, 1.5f, 0.5f)));
    emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-0.5f, 1.0f, -0.5f), Vector3(0.5f, 3.0f, 0.5f)));
    emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.8f, 1.5f));
    emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(0.05f, 0.05f, 0.05f), Vector3(0.15f, 0.15f, 0.15f)));
    emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.6f, 0.2f, 0.8f, 1.0f)));
    emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(0.6f, 0.2f, 0.8f, 1.0f), Vector4(0.8f, 0.4f, 1.0f, 0.0f)));
    
    ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
}

void EnemyDeathEffect::InitializeSmokeEmitter()
{
    auto emitter = std::make_unique<ParticleEmitter>();
    emitter->Initialize("enemy_smoke");
    
    auto renderer = std::make_unique<SpriteRenderer>();
    renderer->Initialize(smokeTexturePath_);
    renderer->SetBlendMode(BlendMode::Alpha);
    emitter->SetRenderer(std::move(renderer));
    
    emitter->AddModule(std::make_unique<SpawnBurstModule>(5, 0.1f));
    emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.3f, 0.0f, -0.3f), Vector3(0.3f, 0.5f, 0.3f)));
    emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-1.0f, 1.0f, -1.0f), Vector3(1.0f, 3.0f, 1.0f)));
    emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.5f, 1.0f));
    emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(0.2f, 0.2f, 0.2f), Vector3(0.5f, 0.5f, 0.5f)));
    emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.3f, 0.3f, 0.3f, 0.5f)));
    emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(0.3f, 0.3f, 0.3f, 0.5f), Vector4(0.5f, 0.5f, 0.5f, 0.0f)));
    emitter->AddModule(std::make_unique<ScaleOverLifetimeModule>(Vector3(0.3f, 0.3f, 0.3f), Vector3(0.8f, 0.8f, 0.8f)));
    
    ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
}

void EnemyDeathEffect::PlayDeathEffect(const Vector3& position, EffectType type)
{
    switch(type)
    {
        case EffectType::Normal:
        {
            auto* blood = ParticleManager::GetInstance()->GetEmitter("enemy_blood");
            auto* fragment = ParticleManager::GetInstance()->GetEmitter("enemy_fragment");
            auto* smoke = ParticleManager::GetInstance()->GetEmitter("enemy_smoke");
            if (blood) blood->SetPosition(position);
            if (fragment) fragment->SetPosition(position);
            if (smoke) smoke->SetPosition(position);
            break;
        }
        case EffectType::Explosive:
            PlayExplosionEffect(position);
            break;
        case EffectType::Electric:
            PlayElectricEffect(position);
            break;
        case EffectType::Dissolve:
            PlayDissolveEffect(position);
            break;
    }
}

void EnemyDeathEffect::PlayExplosionEffect(const Vector3& position, float scale)
{
    auto* explosion = ParticleManager::GetInstance()->GetEmitter("enemy_explosion");
    auto* fragment = ParticleManager::GetInstance()->GetEmitter("enemy_fragment");
    auto* smoke = ParticleManager::GetInstance()->GetEmitter("enemy_smoke");
    
    if (explosion) explosion->SetPosition(position);
    if (fragment) fragment->SetPosition(position);
    if (smoke) smoke->SetPosition(position);
}

void EnemyDeathEffect::PlayElectricEffect(const Vector3& position)
{
    auto* electric = ParticleManager::GetInstance()->GetEmitter("enemy_electric");
    if (electric) electric->SetPosition(position);
}

void EnemyDeathEffect::PlayDissolveEffect(const Vector3& position)
{
    auto* dissolve = ParticleManager::GetInstance()->GetEmitter("enemy_dissolve");
    if (dissolve) dissolve->SetPosition(position);
}

