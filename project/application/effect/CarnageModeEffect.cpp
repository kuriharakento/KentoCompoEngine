#include "CarnageModeEffect.h"
#include "effects/particle/component/single/ColorFadeOutComponent.h"
#include "effects/particle/component/single/ScaleOverLifetimeComponent.h"
#include "effects/particle/component/single/RotationComponent.h"
#include "effects/particle/component/single/AccelerationComponent.h"
#include "effects/particle/component/single/DragComponent.h"
#include "effects/particle/component/single/RandomInitialVelocityComponent.h"

CarnageModeEffect::CarnageModeEffect() {}
CarnageModeEffect::~CarnageModeEffect() {}

void CarnageModeEffect::Initialize()
{
    // 赤黒炎オーラ（超大型化）
    auraEmitter_ = std::make_unique<ParticleEmitter>();
    auraEmitter_->Initialize("CarnageAura", auraTexturePath_);
    auraEmitter_->SetBlendMode(BlendMode::Additive);
    auraEmitter_->SetInitialLifeTime(0.7f); // 長め
    auraEmitter_->SetInitialScale({ 4.2f, 4.2f, 4.2f }); // 大きく
    auraEmitter_->SetInitialColor({ 1.0f, 0.1f, 0.2f, 1.0f }); // 強めの赤
    auraEmitter_->SetBillborad(true);
    auraEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    auraEmitter_->AddComponent(std::make_shared<ScaleOverLifetimeComponent>(3.8f, 7.0f)); // 拡大
    auraEmitter_->AddComponent(std::make_shared<RotationComponent>(Vector3(0.0f, 0.45f, 0.0f)));
    auraEmitter_->AddComponent(std::make_shared<RandomInitialVelocityComponent>(
        Vector3(-0.8f, 1.2f, -0.8f), Vector3(0.8f, 2.4f, 0.8f)
    ));

    // 黒煙（ダーク感の強調＋拡大）
    smokeEmitter_ = std::make_unique<ParticleEmitter>();
    smokeEmitter_->Initialize("CarnageSmoke", smokeTexturePath_);
    smokeEmitter_->SetBlendMode(BlendMode::Alpha);
    smokeEmitter_->SetInitialLifeTime(2.6f); // 長め
    smokeEmitter_->SetInitialScale({ 3.2f, 3.2f, 3.2f }); // 大きく
    smokeEmitter_->SetInitialColor({ 0.05f, 0.05f, 0.05f, 0.8f }); // 黒
    smokeEmitter_->SetBillborad(true);
    smokeEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    smokeEmitter_->AddComponent(std::make_shared<DragComponent>(0.91f));
    smokeEmitter_->AddComponent(std::make_shared<ScaleOverLifetimeComponent>(3.0f, 6.0f));
    smokeEmitter_->AddComponent(std::make_shared<AccelerationComponent>(Vector3(0.0f, 0.18f, 0.0f)));

    // 移動軌跡（炎＋稲妻、スケールUP）
    trailEmitter_ = std::make_unique<ParticleEmitter>();
    trailEmitter_->Initialize("CarnageTrail", trailTexturePath_);
    trailEmitter_->SetBlendMode(BlendMode::Additive);
    trailEmitter_->SetEmitRate(0.11f); // 多め
    trailEmitter_->SetInitialLifeTime(0.45f);
    trailEmitter_->SetInitialScale({ 2.7f, 2.7f, 2.7f }); // 大きく
    trailEmitter_->SetInitialColor({ 0.8f, 0.0f, 0.0f, 0.95f }); // 赤黒
    trailEmitter_->SetBillborad(true);
    trailEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    trailEmitter_->AddComponent(std::make_shared<DragComponent>(0.85f));
    trailEmitter_->AddComponent(std::make_shared<ScaleOverLifetimeComponent>(2.7f, 4.5f));
    trailEmitter_->AddComponent(std::make_shared<RotationComponent>(Vector3(0.0f, 0.35f, 0.0f)));

    // 稲妻（赤黒の閃光、大・多め）
    lightningEmitter_ = std::make_unique<ParticleEmitter>();
    lightningEmitter_->Initialize("CarnageLightning", lightningTexturePath_);
    lightningEmitter_->SetBlendMode(BlendMode::Additive);
    lightningEmitter_->SetEmitRate(0.07f);
    lightningEmitter_->SetInitialLifeTime(0.32f);
    lightningEmitter_->SetInitialScale({ 2.1f, 2.1f, 2.1f });
    lightningEmitter_->SetInitialColor({ 0.7f, 0.0f, 0.0f, 0.93f });
    lightningEmitter_->SetBillborad(true);
    lightningEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    lightningEmitter_->AddComponent(std::make_shared<ScaleOverLifetimeComponent>(1.8f, 3.7f));
    lightningEmitter_->AddComponent(std::make_shared<RandomInitialVelocityComponent>(
        Vector3(-3.7f, 1.8f, -3.7f), Vector3(3.7f, 4.2f, 3.7f)
    ));

    // 終了バースト（爆発的な炎＋黒煙＋稲妻、大・多め）
    burstEmitter_ = std::make_unique<ParticleEmitter>();
    burstEmitter_->Initialize("CarnageBurst", burstTexturePath_);
    burstEmitter_->SetBlendMode(BlendMode::Additive);
    burstEmitter_->SetInitialLifeTime(1.25f);
    burstEmitter_->SetInitialScale({ 7.2f, 7.2f, 7.2f });
    burstEmitter_->SetInitialColor({ 0.8f, 0.0f, 0.0f, 0.95f });
    burstEmitter_->SetBillborad(true);
    burstEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    burstEmitter_->AddComponent(std::make_shared<ScaleOverLifetimeComponent>(6.8f, 12.0f));
    burstEmitter_->AddComponent(std::make_shared<RandomInitialVelocityComponent>(
        Vector3(-7.0f, 3.5f, -7.0f), Vector3(7.0f, 8.0f, 7.0f)
    ));
}

void CarnageModeEffect::PlayAuraEffect(const Vector3& position)
{
    // 爆発的な赤黒炎オーラ＋黒煙
    auraEmitter_->Start(&position, 32, 0.0f, false); // 32個・超大型
    smokeEmitter_->Start(&position, 24, 0.0f, false);
}

void CarnageModeEffect::PlayTrailEffect(const Vector3& position, const Vector3& direction)
{
    // 炎の軌跡
    trailEmitter_->Start(&position, 12, 0.01f, true); // 多め
    // 稲妻の閃光も同時に
    lightningEmitter_->Start(&position, 6, 0.02f, true);
}

void CarnageModeEffect::PlayEndEffect(const Vector3& position)
{
    // 爆発的大型バースト
    burstEmitter_->Start(&position, 32, 0.07f, false);
    smokeEmitter_->Start(&position, 24, 0.07f, false);
    lightningEmitter_->Start(&position, 12, 0.05f, false);
}

void CarnageModeEffect::Update(float deltaTime)
{
    // 必要なら毎フレームエミッター更新
    // auraEmitter_->Update(deltaTime);
    // smokeEmitter_->Update(deltaTime);
    // trailEmitter_->Update(deltaTime);
    // lightningEmitter_->Update(deltaTime);
    // burstEmitter_->Update(deltaTime);
}