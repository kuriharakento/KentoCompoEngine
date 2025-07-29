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
    // 残像エミッタの初期化
    afterImageEmitter_ = std::make_unique<ParticleEmitter>();
    afterImageEmitter_->Initialize("dodge_afterimage", afterImageTexturePath_);
    afterImageEmitter_->SetEmitRate(0.03f);  // 残像の生成間隔（短く）
    afterImageEmitter_->SetEmitCount(1);     // 1回に1つの残像
    afterImageEmitter_->SetInitialLifeTime(0.2f); // 短いライフタイム
    afterImageEmitter_->SetInitialScale({ 1.5f, 2.5f, 1.0f }); // キャラクターに合わせたスケール
    afterImageEmitter_->SetInitialColor(VectorColorCodes::White);
    afterImageEmitter_->SetBillborad(false); // 回転を固定（キャラクターの向きに合わせる）

    // フェードアウトコンポーネントの追加（徐々に消える）
    afterImageEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    // 速度減衰コンポーネント（動きを遅くする）
    afterImageEmitter_->AddComponent(std::make_shared<DragComponent>(0.95f));

    // 軌跡エミッタの初期化
    trailEmitter_ = std::make_unique<ParticleEmitter>();
    trailEmitter_->Initialize("dodge_trail", trailTexturePath_);
    trailEmitter_->SetEmitRate(0.01f);  // 頻繁に放出
    trailEmitter_->SetEmitCount(3);     // 一度に複数のパーティクル
    trailEmitter_->SetInitialLifeTime(0.2f);
    trailEmitter_->SetInitialScale({ 0.5f, 0.5f, 0.5f });
    trailEmitter_->SetInitialColor(VectorColorCodes::Black);
    trailEmitter_->SetBillborad(true);  // ビルボードで表示

    // ランダムな初期速度を設定
    trailEmitter_->SetRandomVelocity(true);
    trailEmitter_->SetRandomVelocityRange(AABB({ -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f }));
    // ランダムなスケールを設定
    trailEmitter_->SetRandomScale(true);
    trailEmitter_->SetRandomScaleRange(AABB({ 0.3f, 0.3f, 0.3f }, { 0.8f, 0.8f, 0.8f }));

    // 色のフェードアウト
    trailEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    // 空気抵抗による減速
    trailEmitter_->AddComponent(std::make_shared<DragComponent>(0.9f));
    // 若干上昇する効果
    trailEmitter_->AddComponent(std::make_shared<AccelerationComponent>(Vector3(0.0f, 0.05f, 0.0f)));

    // バーストエミッタの初期化（回避開始時の爆発的なエフェクト）
    burstEmitter_ = std::make_unique<ParticleEmitter>();
    burstEmitter_->Initialize("dodge_burst", burstTexturePath_);
    burstEmitter_->SetEmitCount(15);    // 一度にたくさん放出
    burstEmitter_->SetInitialLifeTime(0.5f);
    burstEmitter_->SetInitialScale({ 0.6f, 0.6f, 0.6f });
    burstEmitter_->SetInitialColor(VectorColorCodes::White);
    burstEmitter_->SetBillborad(true);

    // ランダムなパラメータ設定
    burstEmitter_->SetRandomVelocity(true);
    burstEmitter_->SetRandomVelocityRange(AABB({ -3.0f, 0.0f, -3.0f }, { 3.0f, 2.0f, 3.0f }));
    burstEmitter_->SetRandomScale(true);
    burstEmitter_->SetRandomScaleRange(AABB({ 0.4f, 0.4f, 0.4f }, { 1.2f, 1.2f, 1.2f }));

    // フェードアウト効果
    burstEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    // 重力効果（少し落下する）
    burstEmitter_->AddComponent(std::make_shared<AccelerationComponent>(Vector3(0.0f, -0.05f, 0.0f)));
}

void DodgeEffectParticle::PlayEffect(const Vector3& position, const Vector3& direction)
{
    // 開始時のバーストエフェクト発生
    Vector3 burstPos = position;
    burstPos.y += 1.0f; // 足元から少し上に
    burstEmitter_->Start(burstPos, 15, 0.1f);

    // 軌跡エフェクトを開始（プレイヤーの位置を追跡）
    Vector3* positionPtr = const_cast<Vector3*>(&position);
    trailEmitter_->Start(positionPtr, 3, 0.3f, true);
}

void DodgeEffectParticle::CreateAfterImage(const Vector3& position, const Vector3& rotation)
{
    // 残像の作成（止まっているオブジェクトとして、その場で徐々に消える）
    Vector3 afterImagePos = position;
    afterImagePos.y += 1.0f; // 足元より少し上
    afterImageEmitter_->Start(afterImagePos, 1, 0.05f);

    // 残像の向きを設定（これには追加実装が必要な可能性あり）
    // 例: afterImageEmitter_->SetInitialRotation(rotation);
}

void DodgeEffectParticle::PlayFadeOutEffect(const Vector3& position)
{
    // 軌跡エフェクトを停止
    trailEmitter_->StopEmit();
}