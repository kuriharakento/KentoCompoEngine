#include "TitleFireEffect.h"
#include "effects/particle/ParticleManager.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/update/UpdateModules.h"
#include "time/TimeManager.h"

namespace
{
// 炎エフェクト設定
constexpr float kFireSpawnRate = 10.0f;
constexpr float kFireDragCoefficient = 0.13f;

// 床面エフェクト設定
constexpr float kFloorSpawnRate = 20.0f;
constexpr float kFloorDragCoefficient = 0.05f;

// 配置設定
constexpr float kFireForwardOffset = 10.0f;
}

void TitleFireEffect::Initialize()
{
    // 左側の炎エミッターの初期化
    fireEmitterLeft_ = std::make_unique<ParticleEmitter>();
    fireEmitterLeft_->Initialize("TitleFire_Left");
    
    // レンダラー設定（加算合成で赤く燃える炎）
    auto leftRenderer = std::make_unique<SpriteRenderer>();
    leftRenderer->Initialize(fireTexturePath_);
    leftRenderer->SetBlendMode(BlendMode::Additive);
    fireEmitterLeft_->SetRenderer(std::move(leftRenderer));
    
    // モジュール追加（連続生成、上向きの速度、重力で上昇）
    fireEmitterLeft_->AddModule(std::make_unique<SpawnRateModule>(kFireSpawnRate));
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
    fireEmitterLeft_->AddModule(std::make_unique<DragModule>(kFireDragCoefficient));
    fireEmitterLeft_->AddModule(std::make_unique<GravityModule>(Vector3(0.0f, 1.0f, 0.0f)));
    
    // 右側の炎エミッターの初期化
    fireEmitterRight_ = std::make_unique<ParticleEmitter>();
    fireEmitterRight_->Initialize("TitleFire_Right");
    
    // レンダラー設定（左側と同じ設定）
    auto rightRenderer = std::make_unique<SpriteRenderer>();
    rightRenderer->Initialize(fireTexturePath_);
    rightRenderer->SetBlendMode(BlendMode::Additive);
    fireEmitterRight_->SetRenderer(std::move(rightRenderer));
    
    // モジュール追加（左側と同様のパラメータ）
    fireEmitterRight_->AddModule(std::make_unique<SpawnRateModule>(kFireSpawnRate));
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
    fireEmitterRight_->AddModule(std::make_unique<DragModule>(kFireDragCoefficient));
    fireEmitterRight_->AddModule(std::make_unique<GravityModule>(Vector3(0.0f, 1.0f, 0.0f)));

    // 床面エフェクトの初期化
    floorEmitter_ = std::make_unique<ParticleEmitter>();
    floorEmitter_->Initialize("TitleFloorParticle");
    
    // レンダラー設定（加算合成で床に光る粒子）
    auto floorRenderer = std::make_unique<SpriteRenderer>();
    floorRenderer->Initialize("./Resources/circle2.png");
    floorRenderer->SetBlendMode(BlendMode::Additive);
    floorEmitter_->SetRenderer(std::move(floorRenderer));
    
    // モジュール追加（広範囲に連続生成、上向きの重力）
    floorEmitter_->AddModule(std::make_unique<SpawnRateModule>(kFloorSpawnRate));
    floorEmitter_->AddModule(std::make_unique<InitialPositionModule>(
        Vector3(-30.0f, 0.0f, 0.0f), Vector3(30.0f, 1.0f, 60.0f)));
    floorEmitter_->AddModule(std::make_unique<InitialLifetimeModule>(0.8f, 1.2f));
    floorEmitter_->AddModule(std::make_unique<InitialScaleModule>(
        Vector3(0.001f, 0.001f, 0.001f), Vector3(0.2f, 0.2f, 0.2f)));
    floorEmitter_->AddModule(std::make_unique<InitialColorModule>(Vector4(0.8f, 0.2f, 0.2f, 1.0f)));
    floorEmitter_->AddModule(std::make_unique<ColorFadeModule>(
        Vector4(0.8f, 0.2f, 0.2f, 1.0f), Vector4(0.8f, 0.2f, 0.2f, 0.0f)));
    floorEmitter_->AddModule(std::make_unique<DragModule>(kFloorDragCoefficient));
    floorEmitter_->AddModule(std::make_unique<GravityModule>(Vector3(0.0f, 0.3f, 0.0f)));

    // ParticleManagerに登録
    ParticleManager::GetInstance()->AddEmitter(std::move(fireEmitterLeft_));
    ParticleManager::GetInstance()->AddEmitter(std::move(fireEmitterRight_));
    ParticleManager::GetInstance()->AddEmitter(std::move(floorEmitter_));
    
    firstUpdate_ = false;
}

void TitleFireEffect::Update(const Vector3& cameraPos)
{
    // 床面エフェクトをカメラ位置に追従
    floorPos_ = cameraPos;
    floorPos_.y = groundY_;
    
    // 床面エミッターの位置を更新
    auto* floorEmitter = ParticleManager::GetInstance()->GetEmitter("TitleFloorParticle");
    if (floorEmitter)
    {
        floorEmitter->SetPosition(floorPos_);
    }
    
    // タイマー更新と炎発生判定
    float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;
    if (time_ > 0.0f)
    {
        time_ -= deltaTime;
    }
    else
    {
        // タイマーリセットと炎発生
        time_ = interval_;
        EmitFire(cameraPos);
        lastFireZ_ = cameraPos.z;
    }
}

void TitleFireEffect::EmitFire(const Vector3& position)
{
    // カメラ前方の左右位置に炎を配置
    Vector3 leftPos = position + Vector3(-laneOffset_, groundY_, kFireForwardOffset);
    Vector3 rightPos = position + Vector3(laneOffset_, groundY_, kFireForwardOffset);
    
    // エミッターを取得して位置を設定
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
