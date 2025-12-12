#include "TitleFireEffect.h"
#include "effects/particle/ParticleManager.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/update/UpdateModules.h"
#include "time/TimeManager.h"

void TitleFireEffect::Initialize()
{
    // 蟾ｦ蛛ｴ縺ｮ轤取浤繧ｨ繝溘ャ繧ｿ繝ｼ
    fireEmitterLeft_ = std::make_unique<ParticleEmitter>();
    fireEmitterLeft_->Initialize("TitleFire_Left");
    
    // 繝ｬ繝ｳ繝繝ｩ繝ｼ險ｭ螳・
    auto leftRenderer = std::make_unique<SpriteRenderer>();
    leftRenderer->Initialize(fireTexturePath_);
    leftRenderer->SetBlendMode(BlendMode::Additive);
    fireEmitterLeft_->SetRenderer(std::move(leftRenderer));
    
    // 繝｢繧ｸ繝･繝ｼ繝ｫ霑ｽ蜉
    fireEmitterLeft_->AddModule(std::make_unique<SpawnRateModule>(10.0f)); // 豈守ｧ・0繝代・繝・ぅ繧ｯ繝ｫ
    fireEmitterLeft_->AddModule(std::make_unique<InitialPositionModule>(
        Vector3(-0.3f, 0.0f, -2.0f), Vector3(0.3f, 0.0f, 2.0f)));
    fireEmitterLeft_->AddModule(std::make_unique<InitialVelocityModule>(
        Vector3(-0.5f, 0.5f, -0.5f), Vector3(0.5f, 2.0f, 0.5f)));
    fireEmitterLeft_->AddModule(std::make_unique<InitialLifetimeModule>(0.5f, 0.7f));
    fireEmitterLeft_->AddModule(std::make_unique<InitialScaleModule>(
        Vector3(0.01f, 0.01f, 0.01f), Vector3(0.2f, 0.2f, 0.2f)));
    fireEmitterLeft_->AddModule(std::make_unique<InitialColorModule>(Vector4(0.8f, 0.1f, 0.1f, 0.95f)));
    fireEmitterLeft_->AddModule(std::make_unique<ColorFadeModule>(
        Vector4(0.8f, 0.1f, 0.1f, 0.95f), Vector4(0.8f, 0.1f, 0.1f, 0.0f)));
    fireEmitterLeft_->AddModule(std::make_unique<DragModule>(0.13f)); // 1.0 - 0.87 = 0.13
    fireEmitterLeft_->AddModule(std::make_unique<GravityModule>(Vector3(0.0f, 1.0f, 0.0f))); // 荳頑・
    
    // 蜿ｳ蛛ｴ縺ｮ轤取浤繧ｨ繝溘ャ繧ｿ繝ｼ
    fireEmitterRight_ = std::make_unique<ParticleEmitter>();
    fireEmitterRight_->Initialize("TitleFire_Right");
    
    auto rightRenderer = std::make_unique<SpriteRenderer>();
    rightRenderer->Initialize(fireTexturePath_);
    rightRenderer->SetBlendMode(BlendMode::Additive);
    fireEmitterRight_->SetRenderer(std::move(rightRenderer));
    
    fireEmitterRight_->AddModule(std::make_unique<SpawnRateModule>(10.0f));
    fireEmitterRight_->AddModule(std::make_unique<InitialPositionModule>(
        Vector3(-0.3f, 0.0f, -2.0f), Vector3(0.3f, 0.0f, 2.0f)));
    fireEmitterRight_->AddModule(std::make_unique<InitialVelocityModule>(
        Vector3(-0.5f, 0.5f, -0.5f), Vector3(0.5f, 2.0f, 0.5f)));
    fireEmitterRight_->AddModule(std::make_unique<InitialLifetimeModule>(0.4f, 0.6f));
    fireEmitterRight_->AddModule(std::make_unique<InitialScaleModule>(
        Vector3(0.01f, 0.01f, 0.01f), Vector3(0.2f, 0.2f, 0.2f)));
    fireEmitterRight_->AddModule(std::make_unique<InitialColorModule>(Vector4(0.8f, 0.1f, 0.1f, 0.95f)));
    fireEmitterRight_->AddModule(std::make_unique<ColorFadeModule>(
        Vector4(0.8f, 0.1f, 0.1f, 0.95f), Vector4(0.8f, 0.1f, 0.1f, 0.0f)));
    fireEmitterRight_->AddModule(std::make_unique<DragModule>(0.13f));
    fireEmitterRight_->AddModule(std::make_unique<GravityModule>(Vector3(0.0f, 1.0f, 0.0f)));

    // 蠎企擇繧ｨ繝輔ぉ繧ｯ繝・
    floorEmitter_ = std::make_unique<ParticleEmitter>();
    floorEmitter_->Initialize("TitleFloorParticle");
    
    auto floorRenderer = std::make_unique<SpriteRenderer>();
    floorRenderer->Initialize("./Resources/circle2.png");
    floorRenderer->SetBlendMode(BlendMode::Additive);
    floorEmitter_->SetRenderer(std::move(floorRenderer));
    
    floorEmitter_->AddModule(std::make_unique<SpawnRateModule>(20.0f));
    floorEmitter_->AddModule(std::make_unique<InitialPositionModule>(
        Vector3(-30.0f, 0.0f, 0.0f), Vector3(30.0f, 1.0f, 60.0f)));
    floorEmitter_->AddModule(std::make_unique<InitialLifetimeModule>(0.8f, 1.2f));
    floorEmitter_->AddModule(std::make_unique<InitialScaleModule>(
        Vector3(0.001f, 0.001f, 0.001f), Vector3(0.2f, 0.2f, 0.2f)));
    floorEmitter_->AddModule(std::make_unique<InitialColorModule>(Vector4(0.8f, 0.2f, 0.2f, 1.0f)));
    floorEmitter_->AddModule(std::make_unique<ColorFadeModule>(
        Vector4(0.8f, 0.2f, 0.2f, 1.0f), Vector4(0.8f, 0.2f, 0.2f, 0.0f)));
    floorEmitter_->AddModule(std::make_unique<DragModule>(0.05f));
    floorEmitter_->AddModule(std::make_unique<GravityModule>(Vector3(0.0f, 0.3f, 0.0f)));

    // ParticleManager縺ｫ逋ｻ骭ｲ
    ParticleManager::GetInstance()->AddEmitter(std::move(fireEmitterLeft_));
    ParticleManager::GetInstance()->AddEmitter(std::move(fireEmitterRight_));
    ParticleManager::GetInstance()->AddEmitter(std::move(floorEmitter_));
    
    firstUpdate_ = false;
}

void TitleFireEffect::Update(const Vector3& cameraPos)
{
    // 蠎企擇繧ｨ繝輔ぉ繧ｯ繝医ｒ繧ｫ繝｡繝ｩ菴咲ｽｮ縺ｫ霑ｽ蠕・
    floorPos_ = cameraPos;
    floorPos_.y = groundY_;
    
    // 蠎企擇繧ｨ繝溘ャ繧ｿ繝ｼ縺ｮ菴咲ｽｮ繧呈峩譁ｰ
    auto* floorEmitter = ParticleManager::GetInstance()->GetEmitter("TitleFloorParticle");
    if (floorEmitter)
    {
        floorEmitter->SetPosition(floorPos_);
    }
    
    // 繧ｿ繧､繝槭・譖ｴ譁ｰ縺ｨ轤守匱逕溷愛螳・
    float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;
    if (time_ > 0.0f)
    {
        time_ -= deltaTime;
    }
    else
    {
        time_ = interval_;
        EmitFire(cameraPos);
        lastFireZ_ = cameraPos.z;
    }
}

void TitleFireEffect::EmitFire(const Vector3& position)
{
    // 繧ｫ繝｡繝ｩ蜑肴婿縺ｮ蟾ｦ蜿ｳ菴咲ｽｮ縺ｫ轤取浤繧帝・鄂ｮ
    Vector3 leftPos = position + Vector3(-laneOffset_, groundY_, 10.0f);
    Vector3 rightPos = position + Vector3(laneOffset_, groundY_, 10.0f);
    
    auto* leftEmitter = ParticleManager::GetInstance()->GetEmitter("TitleFire_Left");
    auto* rightEmitter = ParticleManager::GetInstance()->GetEmitter("TitleFire_Right");
    
    if (leftEmitter)
    {
        leftEmitter->SetPosition(leftPos);
    }
    if (rightEmitter)
    {
        rightEmitter->SetPosition(rightPos);
    }
}

