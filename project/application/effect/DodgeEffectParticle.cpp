#include "DodgeEffectParticle.h"

#include "effects/particle/ParticleManager.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/update/UpdateModules.h"

DodgeEffectParticle::DodgeEffectParticle() = default;
DodgeEffectParticle::~DodgeEffectParticle() = default;

void DodgeEffectParticle::Initialize()
{
    // 谿句ワ繧ｨ繝溘ャ繧ｿ繝ｼ
    {
        auto emitter = std::make_unique<ParticleEmitter>();
        emitter->Initialize("dodge_afterimage");
        
        auto renderer = std::make_unique<SpriteRenderer>();
        renderer->Initialize(afterImageTexturePath_);
        renderer->SetBlendMode(BlendMode::Additive);
        emitter->SetRenderer(std::move(renderer));
        
        emitter->AddModule(std::make_unique<SpawnRateModule>(30.0f));
        emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.1f, -0.1f, -0.1f), Vector3(0.1f, 0.1f, 0.1f)));
        emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.15f, 0.25f));
        emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(0.4f, 0.4f, 0.4f), Vector3(0.6f, 0.6f, 0.6f)));
        emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(1.0f, 1.0f, 1.0f, 0.8f)));
        emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(1.0f, 1.0f, 1.0f, 0.8f), Vector4(1.0f, 1.0f, 1.0f, 0.0f)));
        emitter->AddModule(std::make_unique<DragModule>(0.05f));
        
        ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
    }

    // 霆瑚ｷ｡繧ｨ繝溘ャ繧ｿ繝ｼ
    {
        auto emitter = std::make_unique<ParticleEmitter>();
        emitter->Initialize("dodge_trail");
        
        auto renderer = std::make_unique<SpriteRenderer>();
        renderer->Initialize(trailTexturePath_);
        renderer->SetBlendMode(BlendMode::Additive);
        emitter->SetRenderer(std::move(renderer));
        
        emitter->AddModule(std::make_unique<SpawnRateModule>(100.0f));
        emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.2f, -0.2f, -0.2f), Vector3(0.2f, 0.2f, 0.2f)));
        emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f)));
        emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.15f, 0.25f));
        emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(0.3f, 0.3f, 0.3f), Vector3(0.8f, 0.8f, 0.8f)));
        emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.1f, 0.1f, 0.1f, 1.0f)));
        emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(0.1f, 0.1f, 0.1f, 1.0f), Vector4(0.0f, 0.0f, 0.0f, 0.0f)));
        emitter->AddModule(std::make_unique<DragModule>(0.1f));
        emitter->AddModule(std::make_unique<GravityModule>(Vector3(0.0f, 0.05f, 0.0f)));
        
        ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
    }

    // 繝舌・繧ｹ繝医お繝溘ャ繧ｿ繝ｼ
    {
        auto emitter = std::make_unique<ParticleEmitter>();
        emitter->Initialize("dodge_burst");
        
        auto renderer = std::make_unique<SpriteRenderer>();
        renderer->Initialize(burstTexturePath_);
        renderer->SetBlendMode(BlendMode::Additive);
        emitter->SetRenderer(std::move(renderer));
        
        emitter->AddModule(std::make_unique<SpawnBurstModule>(15, 0.5f));
        emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.3f, -0.3f, -0.3f), Vector3(0.3f, 0.3f, 0.3f)));
        emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-3.0f, 0.0f, -3.0f), Vector3(3.0f, 2.0f, 3.0f)));
        emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.4f, 0.6f));
        emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(0.4f, 0.4f, 0.4f), Vector3(1.2f, 1.2f, 1.2f)));
        emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(1.0f, 1.0f, 1.0f, 1.0f)));
        emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(1.0f, 1.0f, 1.0f, 0.0f)));
        emitter->AddModule(std::make_unique<GravityModule>(Vector3(0.0f, -0.05f, 0.0f)));
        
        ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
    }
}

void DodgeEffectParticle::PlayEffect(const Vector3& position, const Vector3& direction)
{
    Vector3 burstPos = position;
    burstPos.y += 1.0f;
    
    auto* burst = ParticleManager::GetInstance()->GetEmitter("dodge_burst");
    auto* trail = ParticleManager::GetInstance()->GetEmitter("dodge_trail");
    
    if (burst) burst->SetPosition(burstPos);
    if (trail) trail->SetPosition(position);
}

void DodgeEffectParticle::CreateAfterImage(const Vector3& position, const Vector3& rotation)
{
    Vector3 afterImagePos = position;
    afterImagePos.y += 1.0f;
    
    auto* afterImage = ParticleManager::GetInstance()->GetEmitter("dodge_afterimage");
    if (afterImage) afterImage->SetPosition(afterImagePos);
}

void DodgeEffectParticle::PlayFadeOutEffect(const Vector3& position)
{
    // NPS縺ｧ縺ｯ迴ｾ蝨ｨStopEmit逶ｸ蠖薙・讖溯・縺後↑縺・◆繧√∽ｽ咲ｽｮ繧偵Μ繧ｻ繝・ヨ
    auto* trail = ParticleManager::GetInstance()->GetEmitter("dodge_trail");
    if (trail) trail->SetPosition(Vector3(0, -1000, 0)); // 逕ｻ髱｢螟悶↓遘ｻ蜍・
}

