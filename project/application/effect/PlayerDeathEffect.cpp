#include "PlayerDeathEffect.h"
#include "application/GameObject/Combatable/character/player/Player.h"
#include "effects/particle/ParticleManager.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "math/Easing.h"
#include "math/VectorColorCodes.h"
#include "time/TimeManager.h"
#include <effects/particle/module/spawn/SpawnModules.h>
#include <effects/particle/module/spawn/InitialModules.h>
#include <effects/particle/module/update/UpdateModules.h>

namespace
{
// バーストエフェクト設定
constexpr int kDebrisBurstCount = 10;
constexpr float kDebrisBurstDuration = 0.1f;

// パーティクル開始のタイミング
constexpr float kParticleStartThreshold = 0.5f;
}

void PlayerDeathEffect::Initialize(Player* player)
{
// プレイヤーへの参照と初期スケールを保存
player_ = player;
initialScale_ = player->GetScale();

// 破片エミッターの作成と初期化
auto emitter = std::make_unique<ParticleEmitter>();
emitter->Initialize(emitterName_);

// レンダラー設定（加算合成で光る星型パーティクル）
auto renderer = std::make_unique<SpriteRenderer>();
renderer->Initialize("./Resources/star.png");
renderer->SetBlendMode(BlendMode::Additive);
emitter->SetRenderer(std::move(renderer));

// モジュール追加（バースト生成で爆発的に飛散）
emitter->AddModule(std::make_unique<SpawnBurstModule>(kDebrisBurstCount, kDebrisBurstDuration));
emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.5f, -0.5f, -0.5f), Vector3(0.5f, 0.5f, 0.5f)));
emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-8.0f, -8.0f, -8.0f), Vector3(8.0f, 8.0f, 8.0f)));
emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.5f, 0.9f));
emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(5.0f, 5.0f, 5.0f), Vector3(30.0f, 30.0f, 30.0f)));
emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.0f, 1.0f, 1.0f, 1.0f))); // Cyan
emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(1.0f, 0.0f, 0.0f, 1.0f), Vector4(1.0f, 0.0f, 0.0f, 0.0f)));

// マネージャーに登録
ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
}

void PlayerDeathEffect::Update()
{
// プレイヤーが存在しない、またはエフェクトが非アクティブな場合は早期リターン
if (!player_ || !isActive_)
{
return;
}

// デルタタイムの取得
float delta = TimeManager::GetInstance().GetGameContext().deltaTime;

// 経過時間を更新し、進行度を計算
elapsed_ += delta;
float t = std::clamp(elapsed_ / duration_, 0.0f, 1.0f);

// イージング関数を使用してプレイヤーのスケールを縮小
Vector3 newScale = {
EasingToEnd(initialScale_.x, 0.0f, EaseInOutQuint, t),
EasingToEnd(initialScale_.y, 0.0f, EaseInOutQuint, t),
EasingToEnd(initialScale_.z, 0.0f, EaseInOutQuint, t)
};
player_->SetScale(newScale);

// 進行度が50%を超えたらパーティクルを発生
if(t > kParticleStartThreshold && !isParticleStarted_)
{
auto* emitter = ParticleManager::GetInstance()->GetEmitter(emitterName_);
if (emitter)
{
emitter->SetPosition(player_->GetPosition());
}
isParticleStarted_ = true;
}
}

void PlayerDeathEffect::Play(float duration)
{
// エフェクトをアクティブにして持続時間を設定
isActive_ = true;
duration_ = duration;
}

bool PlayerDeathEffect::IsFinished() const
{
// 経過時間が持続時間以上になったら完了
return elapsed_ >= duration_;
}
