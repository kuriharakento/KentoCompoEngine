#include "PlayerDeathEffect.h"

#include "application/GameObject/Combatable/character/player/Player.h"
#include "effects/particle/component/single/ColorFadeOutComponent.h"
#include "effects/particle/component/single/RotationComponent.h"
#include "math/Easing.h"
#include "math/VectorColorCodes.h"
#include "time/TimeManager.h"

void PlayerDeathEffect::Initialize(Player* player)
{
	player_ = player;
	initialScale_ = player->GetScale();

	// 破片エミッターの初期化（キューブ型の3Dパーティクルを使用）
	debrisEmitter_ = std::make_unique<ParticleEmitter>();
	debrisEmitter_->Initialize(
		"player_death_debris",
		"./Resources/star.png"
	);
	
	// パーティクルの基本設定
	debrisEmitter_->SetInitialLifeTime(0.7f);
	debrisEmitter_->SetEmitRate(0.1f);
	debrisEmitter_->SetModelType(ParticleGroup::ParticleType::Cube);
	debrisEmitter_->SetBlendMode(BlendMode::Additive);
	
	// ランダムパラメータ設定（多様な破片表現）
	debrisEmitter_->SetRandomScale(true);
	debrisEmitter_->SetRandomScaleRange(AABB({ 5.0f, 5.0f, 5.0f }, { 30.0f, 30.0f, 30.0f }));
	debrisEmitter_->SetRandomVelocity(true);
	debrisEmitter_->SetRandomVelocityRange(AABB({ -8.0f, -8.0f, -8.0f }, { 8.0f, 8.0f, 8.0f }));
	debrisEmitter_->SetRandomRotation(true);
	debrisEmitter_->SetRandomRotationRange(AABB({ 0.0f, 0.0f, 0.0f }, { 3.14f, 3.14f, 3.14f }));
	debrisEmitter_->SetRandomColor(true);
	debrisEmitter_->SetRandomColorRange(VectorColorCodes::Cyan, VectorColorCodes::Red);
	
	// エフェクトコンポーネントの追加
	debrisEmitter_->AddComponent(std::make_unique<ColorFadeOutComponent>());
	debrisEmitter_->AddComponent(std::make_unique<RotationComponent>(Vector3(0.0f, 0.15f, 0.0f)));
}

void PlayerDeathEffect::Update()
{
	// 無効な状態では処理をスキップ（早期リターンで無駄な処理を回避）
	if (!player_ || !isActive_)
	{
		return;
	}

	float delta = TimeManager::GetInstance().GetGameContext().deltaTime;

	elapsed_ += delta;
	// 進捗度を0～1に正規化
	float t = std::clamp(elapsed_ / duration_, 0.0f, 1.0f);
	
	// イージング関数で滑らかにスケールを0に向かって縮小
	Vector3 newScale = {
		EasingToEnd(initialScale_.x, 0.0f, EaseInOutQuint, t),
		EasingToEnd(initialScale_.y, 0.0f, EaseInOutQuint, t),
		EasingToEnd(initialScale_.z, 0.0f, EaseInOutQuint, t)
	};
	player_->SetScale(newScale);

	// 演出の中盤（50%経過時点）で破片パーティクルを開始
	if(t > 0.5f && !isParticleStarted_)
	{
		debrisEmitter_->Start(
			&player_->GetPosition(),
			10,
			1.0f,
			false
		);

		isParticleStarted_ = true;
	}
	
}

void PlayerDeathEffect::Play(float duration)
{
	isActive_ = true;
	duration_ = duration;
}

bool PlayerDeathEffect::IsFinished() const
{
	return elapsed_ >= duration_;
}
