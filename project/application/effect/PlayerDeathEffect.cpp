#include "PlayerDeathEffect.h"

#include "application/GameObject/Combatable/character/player/Player.h"
#include "effects/particle/component/single/ColorFadeOutComponent.h"
#include "math/Easing.h"
#include "math/VectorColorCodes.h"
#include "time/TimeManager.h"

void PlayerDeathEffect::Initialize(Player* player)
{
	// プレイヤーの参照を保存
	player_ = player;

	// プレイヤーのスケールを保存
	initialScale_ = player->GetScale();

	// エミッターの初期化
	debrisEmitter_ = std::make_unique<ParticleEmitter>();
	debrisEmitter_->Initialize(
		"player_death_debris",
		"./Resources/circle2.png"
	);
	// エミッターの設定を追加
	debrisEmitter_->SetInitialLifeTime(1.0f);
	debrisEmitter_->SetEmitRate(0.1f);
	debrisEmitter_->SetRandomScale(true);
	debrisEmitter_->SetRandomScaleRange(AABB({ 0.1f, 0.1f, 0.1f }, { 0.5f, 0.5f, 0.5f }));
	debrisEmitter_->SetRandomVelocity(true);
	debrisEmitter_->SetRandomVelocityRange(AABB({ -2.0f, 2.0f, -2.0f }, { 2.0f, 2.0f, 2.0f }));
	debrisEmitter_->SetBlendMode(BlendMode::Additive);
	debrisEmitter_->SetBillborad(true);
	debrisEmitter_->SetRandomColor(true);
	debrisEmitter_->SetRandomColorRange(VectorColorCodes::Cyan, VectorColorCodes::Red);
	// コンポーネントを追加
	debrisEmitter_->AddComponent(std::make_unique<ColorFadeOutComponent>());
}

void PlayerDeathEffect::Update()
{
	// プレイヤーが存在しないか、生存している、非アクティブ状態の場合は処理しない
	if (!player_ || player_->IsAlive() || !isActive_)
	{
		return;
	}

	float delta = TimeManager::GetInstance().GetGameContext().deltaTime;

	// 経過時間を更新
	elapsed_ += delta;
	// 進捗度を計算（0～1に正規化）
	float t = std::clamp(elapsed_ / duration_, 0.0f, 1.0f);
	// イージング関数を使用してスケールを補間
	Vector3 newScale = {
		EasingToEnd(initialScale_.x, 0.0f, EaseInOutQuint, t),
		EasingToEnd(initialScale_.y, 0.0f, EaseInOutQuint, t),
		EasingToEnd(initialScale_.z, 0.0f, EaseInOutQuint, t)
	};
	// スケールを適用
	player_->SetScale(newScale);

	// エフェクト開始
	if(t > 0.5f && !isParticleStarted_)
	{
		// エミッターを開始
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
