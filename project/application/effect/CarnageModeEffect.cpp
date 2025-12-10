#include "CarnageModeEffect.h"
#include "effects/particle/ParticleManager.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/update/UpdateModules.h"

CarnageModeEffect::CarnageModeEffect() = default;
CarnageModeEffect::~CarnageModeEffect() = default;

void CarnageModeEffect::Initialize()
{
    // 襍､鮟堤ｎ繧ｪ繝ｼ繝ｩ
    {
        auto emitter = std::make_unique<ParticleEmitter>();
        emitter->Initialize("CarnageAura");
        
        auto renderer = std::make_unique<SpriteRenderer>();
        renderer->Initialize(auraTexturePath_);
        renderer->SetBlendMode(BlendMode::Additive);
        emitter->SetRenderer(std::move(renderer));
        
        emitter->AddModule(std::make_unique<SpawnBurstModule>(32, 0.5f));
        emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-1.0f, 0.0f, -1.0f), Vector3(1.0f, 2.0f, 1.0f)));
        emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-0.8f, 1.2f, -0.8f), Vector3(0.8f, 2.4f, 0.8f)));
        emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.5f, 0.9f));
        emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(3.8f, 3.8f, 3.8f), Vector3(4.6f, 4.6f, 4.6f)));
        emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(1.0f, 0.1f, 0.2f, 1.0f)));
        emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(1.0f, 0.1f, 0.2f, 1.0f), Vector4(0.8f, 0.0f, 0.0f, 0.0f)));
        emitter->AddModule(std::make_unique<ScaleOverLifetimeModule>(Vector3(3.8f, 3.8f, 3.8f), Vector3(7.0f, 7.0f, 7.0f)));
        
        ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
    }

    // 鮟堤・
    {
        auto emitter = std::make_unique<ParticleEmitter>();
        emitter->Initialize("CarnageSmoke");
        
        auto renderer = std::make_unique<SpriteRenderer>();
        renderer->Initialize(smokeTexturePath_);
        renderer->SetBlendMode(BlendMode::Alpha);
        emitter->SetRenderer(std::move(renderer));
        
        emitter->AddModule(std::make_unique<SpawnBurstModule>(24, 0.5f));
        emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.5f, 0.0f, -0.5f), Vector3(0.5f, 1.0f, 0.5f)));
        emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-0.3f, 0.1f, -0.3f), Vector3(0.3f, 0.5f, 0.3f)));
        emitter->AddModule(std::make_unique<InitialLifetimeModule>(2.0f, 3.0f));
        emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(3.0f, 3.0f, 3.0f), Vector3(3.5f, 3.5f, 3.5f)));
        emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.05f, 0.05f, 0.05f, 0.8f)));
        emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(0.05f, 0.05f, 0.05f, 0.8f), Vector4(0.1f, 0.1f, 0.1f, 0.0f)));
        emitter->AddModule(std::make_unique<DragModule>(0.09f));
        emitter->AddModule(std::make_unique<ScaleOverLifetimeModule>(Vector3(3.0f, 3.0f, 3.0f), Vector3(6.0f, 6.0f, 6.0f)));
        
        ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
    }

    // 遘ｻ蜍戊ｻ瑚ｷ｡
    {
        auto emitter = std::make_unique<ParticleEmitter>();
        emitter->Initialize("CarnageTrail");
        
        auto renderer = std::make_unique<SpriteRenderer>();
        renderer->Initialize(trailTexturePath_);
        renderer->SetBlendMode(BlendMode::Additive);
        emitter->SetRenderer(std::move(renderer));
        
        emitter->AddModule(std::make_unique<SpawnRateModule>(9.0f));
        emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.3f, 0.0f, -0.3f), Vector3(0.3f, 1.0f, 0.3f)));
        emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.35f, 0.55f));
        emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(2.5f, 2.5f, 2.5f), Vector3(3.0f, 3.0f, 3.0f)));
        emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.8f, 0.0f, 0.0f, 0.95f)));
        emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(0.8f, 0.0f, 0.0f, 0.95f), Vector4(0.5f, 0.0f, 0.0f, 0.0f)));
        emitter->AddModule(std::make_unique<DragModule>(0.15f));
        emitter->AddModule(std::make_unique<ScaleOverLifetimeModule>(Vector3(2.7f, 2.7f, 2.7f), Vector3(4.5f, 4.5f, 4.5f)));
        
        ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
    }

    // 遞ｲ螯ｻ
    {
        auto emitter = std::make_unique<ParticleEmitter>();
        emitter->Initialize("CarnageLightning");
        
        auto renderer = std::make_unique<SpriteRenderer>();
        renderer->Initialize(lightningTexturePath_);
        renderer->SetBlendMode(BlendMode::Additive);
        emitter->SetRenderer(std::move(renderer));
        
        emitter->AddModule(std::make_unique<SpawnRateModule>(14.0f));
        emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.5f, 0.0f, -0.5f), Vector3(0.5f, 1.5f, 0.5f)));
        emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-3.7f, 1.8f, -3.7f), Vector3(3.7f, 4.2f, 3.7f)));
        emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.2f, 0.4f));
        emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(1.8f, 1.8f, 1.8f), Vector3(2.4f, 2.4f, 2.4f)));
        emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.7f, 0.0f, 0.0f, 0.93f)));
        emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(0.7f, 0.0f, 0.0f, 0.93f), Vector4(1.0f, 0.3f, 0.0f, 0.0f)));
        emitter->AddModule(std::make_unique<ScaleOverLifetimeModule>(Vector3(1.8f, 1.8f, 1.8f), Vector3(3.7f, 3.7f, 3.7f)));
        
        ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
    }

    // 邨ゆｺ・ヰ繝ｼ繧ｹ繝・
    {
        auto emitter = std::make_unique<ParticleEmitter>();
        emitter->Initialize("CarnageBurst");
        
        auto renderer = std::make_unique<SpriteRenderer>();
        renderer->Initialize(burstTexturePath_);
        renderer->SetBlendMode(BlendMode::Additive);
        emitter->SetRenderer(std::move(renderer));
        
        emitter->AddModule(std::make_unique<SpawnBurstModule>(32, 1.0f));
        emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-1.0f, 0.0f, -1.0f), Vector3(1.0f, 2.0f, 1.0f)));
        emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-7.0f, 3.5f, -7.0f), Vector3(7.0f, 8.0f, 7.0f)));
        emitter->AddModule(std::make_unique<InitialLifetimeModule>(1.0f, 1.5f));
        emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(6.8f, 6.8f, 6.8f), Vector3(7.6f, 7.6f, 7.6f)));
        emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.8f, 0.0f, 0.0f, 0.95f)));
        emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(0.8f, 0.0f, 0.0f, 0.95f), Vector4(0.3f, 0.0f, 0.0f, 0.0f)));
        emitter->AddModule(std::make_unique<ScaleOverLifetimeModule>(Vector3(6.8f, 6.8f, 6.8f), Vector3(12.0f, 12.0f, 12.0f)));
        
        ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
    }
}

void CarnageModeEffect::PlayAuraEffect(const Vector3& position)
{
    auto* aura = ParticleManager::GetInstance()->GetEmitter("CarnageAura");
    auto* smoke = ParticleManager::GetInstance()->GetEmitter("CarnageSmoke");
    if (aura) aura->SetPosition(position);
    if (smoke) smoke->SetPosition(position);
}

void CarnageModeEffect::PlayTrailEffect(const Vector3& position, const Vector3& direction)
{
    auto* trail = ParticleManager::GetInstance()->GetEmitter("CarnageTrail");
    auto* lightning = ParticleManager::GetInstance()->GetEmitter("CarnageLightning");
    if (trail) trail->SetPosition(position);
    if (lightning) lightning->SetPosition(position);
}

void CarnageModeEffect::PlayEndEffect(const Vector3& position)
{
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

