#include "DodgeEffectParticle.h"

#include "effects/particle/ParticleManager.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/update/UpdateModules.h"

namespace
{
	// 残像エフェクト設定
	constexpr float kAfterImageSpawnRate = 30.0f;
	constexpr float kAfterImageDragCoefficient = 0.05f;

	// 軌跡エフェクト設定
	constexpr float kTrailSpawnRate = 100.0f;
	constexpr float kTrailDragCoefficient = 0.1f;

	// バーストエフェクト設定
	constexpr int kBurstParticleCount = 15;
	constexpr float kBurstDuration = 0.5f;

	// 位置オフセット
	constexpr float kHeightOffset = 1.0f;

	// 停止時の移動先Y座標
	constexpr float kHiddenPositionY = -1000.0f;
}

DodgeEffectParticle::DodgeEffectParticle() = default;
DodgeEffectParticle::~DodgeEffectParticle() = default;

void DodgeEffectParticle::Initialize()
{
    // 残像エミッターの初期化
    {
        auto emitter = std::make_unique<ParticleEmitter>();
        emitter->Initialize("dodge_afterimage");
        
        // レンダラー設定（加算合成で白く光る残像）
        auto renderer = std::make_unique<SpriteRenderer>();
        renderer->Initialize(afterImageTexturePath_);
        renderer->SetBlendMode(BlendMode::Additive);
        emitter->SetRenderer(std::move(renderer));
        
        // 連続生成とパラメータ設定
        emitter->AddModule(std::make_unique<SpawnRateModule>(kAfterImageSpawnRate));
        emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.1f, -0.1f, -0.1f), Vector3(0.1f, 0.1f, 0.1f)));
        emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.15f, 0.25f));
        emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(0.4f, 0.4f, 0.4f), Vector3(0.6f, 0.6f, 0.6f)));
        emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(1.0f, 1.0f, 1.0f, 0.8f)));
        emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(1.0f, 1.0f, 1.0f, 0.8f), Vector4(1.0f, 1.0f, 1.0f, 0.0f)));
        emitter->AddModule(std::make_unique<DragModule>(kAfterImageDragCoefficient));
        
        // マネージャーに登録
        ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
    }

    // 軌跡エミッターの初期化
    {
        auto emitter = std::make_unique<ParticleEmitter>();
        emitter->Initialize("dodge_trail");
        
        // レンダラー設定（加算合成で暗めの軌跡）
        auto renderer = std::make_unique<SpriteRenderer>();
        renderer->Initialize(trailTexturePath_);
        renderer->SetBlendMode(BlendMode::Additive);
        emitter->SetRenderer(std::move(renderer));
        
        // 高頻度生成とパラメータ設定（重力とドラッグで自然な軌跡）
        emitter->AddModule(std::make_unique<SpawnRateModule>(kTrailSpawnRate));
        emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.2f, -0.2f, -0.2f), Vector3(0.2f, 0.2f, 0.2f)));
        emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f)));
        emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.15f, 0.25f));
        emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(0.3f, 0.3f, 0.3f), Vector3(0.8f, 0.8f, 0.8f)));
        emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.1f, 0.1f, 0.1f, 1.0f)));
        emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(0.1f, 0.1f, 0.1f, 1.0f), Vector4(0.0f, 0.0f, 0.0f, 0.0f)));
        emitter->AddModule(std::make_unique<DragModule>(kTrailDragCoefficient));
        emitter->AddModule(std::make_unique<GravityModule>(Vector3(0.0f, 0.05f, 0.0f)));
        
        // マネージャーに登録
        ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
    }

    // バーストエミッターの初期化
    {
        auto emitter = std::make_unique<ParticleEmitter>();
        emitter->Initialize("dodge_burst");
        
        // レンダラー設定（星型テクスチャで白く輝くバースト）
        auto renderer = std::make_unique<SpriteRenderer>();
        renderer->Initialize(burstTexturePath_);
        renderer->SetBlendMode(BlendMode::Additive);
        emitter->SetRenderer(std::move(renderer));
        
        // バースト生成とパラメータ設定（下向きの重力で落ちていく）
        emitter->AddModule(std::make_unique<SpawnBurstModule>(kBurstParticleCount, kBurstDuration));
        emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.3f, -0.3f, -0.3f), Vector3(0.3f, 0.3f, 0.3f)));
        emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-3.0f, 0.0f, -3.0f), Vector3(3.0f, 2.0f, 3.0f)));
        emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.4f, 0.6f));
        emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(0.4f, 0.4f, 0.4f), Vector3(1.2f, 1.2f, 1.2f)));
        emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(1.0f, 1.0f, 1.0f, 1.0f)));
        emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(1.0f, 1.0f, 1.0f, 0.0f)));
        emitter->AddModule(std::make_unique<GravityModule>(Vector3(0.0f, -0.05f, 0.0f)));
        
        // マネージャーに登録
        ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
    }
}

void DodgeEffectParticle::PlayEffect(const Vector3& position, const Vector3& direction)
{
    Vector3 burstPos = position;
    burstPos.y += kHeightOffset;
    
    auto* burst = ParticleManager::GetInstance()->GetEmitter("dodge_burst");
    auto* trail = ParticleManager::GetInstance()->GetEmitter("dodge_trail");
    
    if (burst) burst->SetPosition(burstPos);
    if (trail) trail->SetPosition(position);
}

void DodgeEffectParticle::CreateAfterImage(const Vector3& position, const Vector3& rotation)
{
    Vector3 afterImagePos = position;
    afterImagePos.y += kHeightOffset;
    
    auto* afterImage = ParticleManager::GetInstance()->GetEmitter("dodge_afterimage");
    if (afterImage) afterImage->SetPosition(afterImagePos);
}

void DodgeEffectParticle::PlayFadeOutEffect(const Vector3& position)
{
    // NPS縺ｧ縺ｯ迴ｾ蝨ｨStopEmit逶ｸ蠖薙・讖溯・縺後↑縺・◆繧√∽ｽ咲ｽｮ繧偵Μ繧ｻ繝・ヨ
    auto* trail = ParticleManager::GetInstance()->GetEmitter("dodge_trail");
    if (trail) trail->SetPosition(Vector3(0, kHiddenPositionY, 0)); // 
}

