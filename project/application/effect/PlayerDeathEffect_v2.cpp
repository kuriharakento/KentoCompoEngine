#include "PlayerDeathEffect.h"

#include "application/GameObject/Combatable/character/player/Player.h"
#include "effects/particle/ParticleManager.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/update/UpdateModules.h"
#include "math/Easing.h"
#include "math/VectorColorCodes.h"
#include "time/TimeManager.h"

void PlayerDeathEffect::Initialize(Player* player)
{
	player_ = player;
	initialScale_ = player->GetScale();

	// パーティクルエミッターの初期化
	auto emitter = std::make_unique<ParticleEmitter>();
	emitter->Initialize(emitterName_);
	
	auto renderer = std::make_unique<SpriteRenderer>();
	renderer->Initialize("./Resources/star.png");
	renderer->SetBlendMode(BlendMode::Additive);
	emitter->SetRenderer(std::move(renderer));
	
	// モジュール追加
	emitter->AddModule(std::make_unique<SpawnBurstModule>(10, 0.1f));
	emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.5f, -0.5f, -0.5f), Vector3(0.5f, 0.5f, 0.5f)));
	emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(-8.0f, -8.0f, -8.0f), Vector3(8.0f, 8.0f, 8.0f)));
	emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.5f, 0.9f));
	emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(5.0f, 5.0f, 5.0f), Vector3(30.0f, 30.0f, 30.0f)));
	emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.0f, 1.0f, 1.0f, 1.0f))); // Cyan
	emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(1.0f, 0.0f, 0.0f, 1.0f), Vector4(1.0f, 0.0f, 0.0f, 0.0f)));
	
	ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
}

void PlayerDeathEffect::Update()
{
	if (!player_ || !isActive_)
	{
		return;
	}

	float delta = TimeManager::GetInstance().GetGameContext().deltaTime;

	elapsed_ += delta;
	float t = std::clamp(elapsed_ / duration_, 0.0f, 1.0f);
	
	Vector3 newScale = {
		EasingToEnd(initialScale_.x, 0.0f, EaseInOutQuint, t),
		EasingToEnd(initialScale_.y, 0.0f, EaseInOutQuint, t),
		EasingToEnd(initialScale_.z, 0.0f, EaseInOutQuint, t)
	};
	player_->SetScale(newScale);

	if(t > 0.5f && !isParticleStarted_)
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
	isActive_ = true;
	duration_ = duration;
}

bool PlayerDeathEffect::IsFinished() const
{
	return elapsed_ >= duration_;
}
