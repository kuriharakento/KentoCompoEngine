#include "CarnageModeEffect.h"
#include "effects/particle/ParticleManager.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/update/UpdateModules.h"

namespace
{
	// オーラエフェクト設定
	constexpr int kAuraBurstCount = 32;
	constexpr float kAuraBurstDuration = 0.5f;

	// 煙エフェクト設定
	constexpr int kSmokeBurstCount = 24;
	constexpr float kSmokeBurstDuration = 0.5f;
	constexpr float kSmokeDragCoefficient = 0.09f;

	// 軌跡エフェクト設定
	constexpr float kTrailSpawnRate = 9.0f;
	constexpr float kTrailDragCoefficient = 0.15f;

	// 稲妻エフェクト設定
	constexpr float kLightningSpawnRate = 14.0f;

	// バーストエフェクト設定
	constexpr int kBurstParticleCount = 32;
	constexpr float kBurstDuration = 1.0f;
}

CarnageModeEffect::CarnageModeEffect() = default;
CarnageModeEffect::~CarnageModeEffect() = default;

void CarnageModeEffect::Initialize()
{
    // 強力なオーラエフェクトの初期化
    {
        auto emitter = std::make_unique<ParticleEmitter>();
        emitter->Initialize("CarnageAura");
        
        // レンダラー設定（加算合成で明るく光る）
        auto renderer = std::make_unique<SpriteRenderer>();
        renderer->Initialize(auraTexturePath_);
        renderer->SetBlendMode(BlendMode::Additive);
        emitter->SetRenderer(std::move(renderer));
        
        // バースト生成とパラメータ設定
        emitter->AddModule(std::make_unique<SpawnBurstModule>(kAuraBurstCount, kAuraBurstDuration));
        emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-1.0f, 0.0f, -1.0f), Vector3(1.0f, 2.0f, 1.0f)));
        emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-0.8f, 1.2f, -0.8f), Vector3(0.8f, 2.4f, 0.8f)));
        emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.5f, 0.9f));
        emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(3.8f, 3.8f, 3.8f), Vector3(4.6f, 4.6f, 4.6f)));
        emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(1.0f, 0.1f, 0.2f, 1.0f)));
        emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(1.0f, 0.1f, 0.2f, 1.0f), Vector4(0.8f, 0.0f, 0.0f, 0.0f)));
        emitter->AddModule(std::make_unique<ScaleOverLifetimeModule>(Vector3(3.8f, 3.8f, 3.8f), Vector3(7.0f, 7.0f, 7.0f)));
        
        // マネージャーに登録
        ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
    }

    // 暗い煙エフェクトの初期化
    {
        auto emitter = std::make_unique<ParticleEmitter>();
        emitter->Initialize("CarnageSmoke");
        
        // レンダラー設定（アルファブレンドで半透明）
        auto renderer = std::make_unique<SpriteRenderer>();
        renderer->Initialize(smokeTexturePath_);
        renderer->SetBlendMode(BlendMode::Alpha);
        emitter->SetRenderer(std::move(renderer));
        
        // バースト生成とパラメータ設定（長いライフタイムでゆっくり拡散）
        emitter->AddModule(std::make_unique<SpawnBurstModule>(kSmokeBurstCount, kSmokeBurstDuration));
        emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.5f, 0.0f, -0.5f), Vector3(0.5f, 1.0f, 0.5f)));
        emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-0.3f, 0.1f, -0.3f), Vector3(0.3f, 0.5f, 0.3f)));
        emitter->AddModule(std::make_unique<InitialLifetimeModule>(2.0f, 3.0f));
        emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(3.0f, 3.0f, 3.0f), Vector3(3.5f, 3.5f, 3.5f)));
        emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.05f, 0.05f, 0.05f, 0.8f)));
        emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(0.05f, 0.05f, 0.05f, 0.8f), Vector4(0.1f, 0.1f, 0.1f, 0.0f)));
        emitter->AddModule(std::make_unique<DragModule>(kSmokeDragCoefficient));
        emitter->AddModule(std::make_unique<ScaleOverLifetimeModule>(Vector3(3.0f, 3.0f, 3.0f), Vector3(6.0f, 6.0f, 6.0f)));
        
        // マネージャーに登録
        ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
    }

    // 移動軌跡エフェクトの初期化
    {
        auto emitter = std::make_unique<ParticleEmitter>();
        emitter->Initialize("CarnageTrail");
        
        // レンダラー設定（加算合成で赤く光る軌跡）
        auto renderer = std::make_unique<SpriteRenderer>();
        renderer->Initialize(trailTexturePath_);
        renderer->SetBlendMode(BlendMode::Additive);
        emitter->SetRenderer(std::move(renderer));
        
        // 連続生成とパラメータ設定
        emitter->AddModule(std::make_unique<SpawnRateModule>(kTrailSpawnRate));
        emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.3f, 0.0f, -0.3f), Vector3(0.3f, 1.0f, 0.3f)));
        emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.35f, 0.55f));
        emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(2.5f, 2.5f, 2.5f), Vector3(3.0f, 3.0f, 3.0f)));
        emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.8f, 0.0f, 0.0f, 0.95f)));
        emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(0.8f, 0.0f, 0.0f, 0.95f), Vector4(0.5f, 0.0f, 0.0f, 0.0f)));
        emitter->AddModule(std::make_unique<DragModule>(kTrailDragCoefficient));
        emitter->AddModule(std::make_unique<ScaleOverLifetimeModule>(Vector3(2.7f, 2.7f, 2.7f), Vector3(4.5f, 4.5f, 4.5f)));
        
        // マネージャーに登録
        ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
    }

    // 稲妻エフェクトの初期化
    {
        auto emitter = std::make_unique<ParticleEmitter>();
        emitter->Initialize("CarnageLightning");
        
        // レンダラー設定（星型テクスチャで激しい光を表現）
        auto renderer = std::make_unique<SpriteRenderer>();
        renderer->Initialize(lightningTexturePath_);
        renderer->SetBlendMode(BlendMode::Additive);
        emitter->SetRenderer(std::move(renderer));
        
        // 連続生成とパラメータ設定（短いライフタイムで素早く散る）
        emitter->AddModule(std::make_unique<SpawnRateModule>(kLightningSpawnRate));
        emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.5f, 0.0f, -0.5f), Vector3(0.5f, 1.5f, 0.5f)));
        emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-3.7f, 1.8f, -3.7f), Vector3(3.7f, 4.2f, 3.7f)));
        emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.2f, 0.4f));
        emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(1.8f, 1.8f, 1.8f), Vector3(2.4f, 2.4f, 2.4f)));
        emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.7f, 0.0f, 0.0f, 0.93f)));
        emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(0.7f, 0.0f, 0.0f, 0.93f), Vector4(1.0f, 0.3f, 0.0f, 0.0f)));
        emitter->AddModule(std::make_unique<ScaleOverLifetimeModule>(Vector3(1.8f, 1.8f, 1.8f), Vector3(3.7f, 3.7f, 3.7f)));
        
        // マネージャーに登録
        ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
    }

    // 終了バーストエフェクトの初期化
    {
        auto emitter = std::make_unique<ParticleEmitter>();
        emitter->Initialize("CarnageBurst");
        
        // レンダラー設定（大規模なバースト効果）
        auto renderer = std::make_unique<SpriteRenderer>();
        renderer->Initialize(burstTexturePath_);
        renderer->SetBlendMode(BlendMode::Additive);
        emitter->SetRenderer(std::move(renderer));
        
        // バースト生成とパラメータ設定（大きく激しい爆発）
        emitter->AddModule(std::make_unique<SpawnBurstModule>(kBurstParticleCount, kBurstDuration));
        emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-1.0f, 0.0f, -1.0f), Vector3(1.0f, 2.0f, 1.0f)));
        emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-7.0f, 3.5f, -7.0f), Vector3(7.0f, 8.0f, 7.0f)));
        emitter->AddModule(std::make_unique<InitialLifetimeModule>(1.0f, 1.5f));
        emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(6.8f, 6.8f, 6.8f), Vector3(7.6f, 7.6f, 7.6f)));
        emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.8f, 0.0f, 0.0f, 0.95f)));
        emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(0.8f, 0.0f, 0.0f, 0.95f), Vector4(0.3f, 0.0f, 0.0f, 0.0f)));
        emitter->AddModule(std::make_unique<ScaleOverLifetimeModule>(Vector3(6.8f, 6.8f, 6.8f), Vector3(12.0f, 12.0f, 12.0f)));
        
        // マネージャーに登録
        ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
    }
}

void CarnageModeEffect::PlayAuraEffect(const Vector3& position)
{
    // オーラと煙のエミッターを取得して位置を設定
    auto* aura = ParticleManager::GetInstance()->GetEmitter("CarnageAura");
    auto* smoke = ParticleManager::GetInstance()->GetEmitter("CarnageSmoke");
    if (aura) aura->SetPosition(position);
    if (smoke) smoke->SetPosition(position);
}

void CarnageModeEffect::PlayTrailEffect(const Vector3& position, const Vector3& direction)
{
    // 軌跡と稲妻のエミッターを取得して位置を設定
    auto* trail = ParticleManager::GetInstance()->GetEmitter("CarnageTrail");
    auto* lightning = ParticleManager::GetInstance()->GetEmitter("CarnageLightning");
    if (trail) trail->SetPosition(position);
    if (lightning) lightning->SetPosition(position);
}

void CarnageModeEffect::PlayEndEffect(const Vector3& position)
{
    // 終了時のバースト、煙、稲妻を同時に発生
    auto* burst = ParticleManager::GetInstance()->GetEmitter("CarnageBurst");
    auto* smoke = ParticleManager::GetInstance()->GetEmitter("CarnageSmoke");
    auto* lightning = ParticleManager::GetInstance()->GetEmitter("CarnageLightning");
    if (burst) burst->SetPosition(position);
    if (smoke) smoke->SetPosition(position);
    if (lightning) lightning->SetPosition(position);
}

void CarnageModeEffect::Update(float deltaTime)
{
    // ParticleManager縺梧峩譁ｰ繧呈球蠖薙☆繧九◆繧√√％縺薙〒縺ｯ菴輔ｂ縺励↑縺・
}

