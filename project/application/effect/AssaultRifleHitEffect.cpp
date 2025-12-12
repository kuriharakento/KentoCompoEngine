#include "AssaultRifleHitEffect.h"
#include "effects/particle/ParticleManager.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/update/UpdateModules.h"

void AssaultRifleHitEffect::Initialize()
{
	emitterName_ = "AssaultRifleHitEffect" + std::to_string(effectCount_++);
	
	auto emitter = std::make_unique<ParticleEmitter>();
	emitter->Initialize(emitterName_);
	
	auto renderer = std::make_unique<SpriteRenderer>();
	renderer->Initialize("./Resources/gradationLine.png");
	renderer->SetBlendMode(BlendMode::Additive);
	emitter->SetRenderer(std::move(renderer));
	
	emitter->AddModule(std::make_unique<SpawnBurstModule>(5, 0.5f));
	emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.1f, -0.1f, -0.1f), Vector3(0.1f, 0.1f, 0.1f)));
	emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.3f, 0.5f));
	emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(0.1f, 0.1f, 0.1f), Vector3(0.3f, 0.3f, 0.3f)));
	emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(1.0f, 0.8f, 0.3f, 1.0f)));
	emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(1.0f, 0.8f, 0.3f, 1.0f), Vector4(1.0f, 0.5f, 0.0f, 0.0f)));
	emitter->AddModule(std::make_unique<ScaleOverLifetimeModule>(Vector3(0.1f, 0.1f, 0.1f), Vector3(3.0f, 3.0f, 3.0f)));
	
	ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
}

void AssaultRifleHitEffect::Play(const Vector3& position)
{
	auto* emitter = ParticleManager::GetInstance()->GetEmitter(emitterName_);
	if (emitter)
	{
		emitter->SetPosition(position);
	}
}


