#include "DodgeEffectParticle.h"

// component
#include "effects/particle/component/single/AccelerationComponent.h"
#include "effects/particle/component/single/ColorFadeOutComponent.h"
#include "effects/particle/component/single/DragComponent.h"
// math
#include "math/VectorColorCodes.h"

DodgeEffectParticle::DodgeEffectParticle()
{
}

DodgeEffectParticle::~DodgeEffectParticle()
{
}

void DodgeEffectParticle::Initialize()
{
    // 残像エミッタの設定（キャラクターの移動跡に薄い残像を残す）
    afterImageEmitter_ = std::make_unique<ParticleEmitter>();
    afterImageEmitter_->Initialize("dodge_afterimage", afterImageTexturePath_);
    afterImageEmitter_->SetBlendMode(BlendMode::Additive);
    afterImageEmitter_->SetEmitRate(0.03f);
    afterImageEmitter_->SetEmitCount(1);
    afterImageEmitter_->SetInitialLifeTime(0.2f);
    afterImageEmitter_->SetInitialScale({ 0.5f, 0.5f, 0.5f });
    afterImageEmitter_->SetInitialColor(VectorColorCodes::White);
    afterImageEmitter_->SetBillborad(false); // ビルボード無効化により、キャラクターの向きを保持

    // 徐々に消える視覚効果を追加
    afterImageEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    // 慣性により徐々に減速する効果
    afterImageEmitter_->AddComponent(std::make_shared<DragComponent>(0.95f));

    // 軌跡エミッタの設定（高速移動中に黒い軌跡を描く）
    trailEmitter_ = std::make_unique<ParticleEmitter>();
    trailEmitter_->Initialize("dodge_trail", trailTexturePath_);
    trailEmitter_->SetBlendMode(BlendMode::Additive);
    trailEmitter_->SetEmitRate(0.01f);
    trailEmitter_->SetEmitCount(3);
    trailEmitter_->SetInitialLifeTime(0.2f);
    trailEmitter_->SetInitialScale({ 0.5f, 0.5f, 0.5f });
    trailEmitter_->SetInitialColor(VectorColorCodes::Black);
    trailEmitter_->SetBillborad(true);

    // ランダムな方向への散らばりを設定
    trailEmitter_->SetRandomVelocity(true);
    trailEmitter_->SetRandomVelocityRange(AABB({ -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f }));
    trailEmitter_->SetRandomScale(true);
    trailEmitter_->SetRandomScaleRange(AABB({ 0.3f, 0.3f, 0.3f }, { 0.8f, 0.8f, 0.8f }));

    trailEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    trailEmitter_->AddComponent(std::make_shared<DragComponent>(0.9f));
    trailEmitter_->AddComponent(std::make_shared<AccelerationComponent>(Vector3(0.0f, 0.05f, 0.0f))); // 軽い浮遊感

    // バーストエミッタの設定（回避開始時の派手な爆発エフェクト）
    burstEmitter_ = std::make_unique<ParticleEmitter>();
    burstEmitter_->Initialize("dodge_burst", burstTexturePath_);
    burstEmitter_->SetEmitCount(15);
    burstEmitter_->SetInitialLifeTime(0.5f);
    burstEmitter_->SetInitialScale({ 0.6f, 0.6f, 0.6f });
    burstEmitter_->SetBlendMode(BlendMode::Additive);
    burstEmitter_->SetInitialColor(VectorColorCodes::White);
    burstEmitter_->SetBillborad(true);

    // 全方向への放射状の動きを設定
    burstEmitter_->SetRandomVelocity(true);
    burstEmitter_->SetRandomVelocityRange(AABB({ -3.0f, 0.0f, -3.0f }, { 3.0f, 2.0f, 3.0f }));
    burstEmitter_->SetRandomScale(true);
    burstEmitter_->SetRandomScaleRange(AABB({ 0.4f, 0.4f, 0.4f }, { 1.2f, 1.2f, 1.2f }));

    burstEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    burstEmitter_->AddComponent(std::make_shared<AccelerationComponent>(Vector3(0.0f, -0.05f, 0.0f))); // 軽い落下
}

void DodgeEffectParticle::PlayEffect(const Vector3& position, const Vector3& direction)
{
    // 足元より少し上の位置でバーストを発生させ、より目立つ演出にする
    Vector3 burstPos = position;
    burstPos.y += 1.0f;
    burstEmitter_->Start(burstPos, 15, 0.1f);

    // 軌跡エフェクトをプレイヤー位置に追従させる（ポインタ渡しで動的追跡）
    Vector3* positionPtr = const_cast<Vector3*>(&position);
    trailEmitter_->Start(positionPtr, 3, 0.3f, true);
}

void DodgeEffectParticle::CreateAfterImage(const Vector3& position, const Vector3& rotation)
{
    // キャラクターの中心付近に残像を配置
    Vector3 afterImagePos = position;
    afterImagePos.y += 1.0f;
    afterImageEmitter_->Start(afterImagePos, 1, 0.05f);

    // 注意: 残像の回転設定には追加実装が必要な場合があります
    // 将来的には afterImageEmitter_->SetInitialRotation(rotation) のような機能追加を検討
}

void DodgeEffectParticle::PlayFadeOutEffect(const Vector3& position)
{
    // 軌跡エフェクトの新規パーティクル放出を停止（既存パーティクルは自然消滅）
    trailEmitter_->StopEmit();
}