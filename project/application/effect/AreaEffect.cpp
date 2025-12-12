#include "AreaEffect.h"
#include "effects/particle/ParticleManager.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/update/UpdateModules.h"

void AreaEffect::Initialize(const Vector3& rotate, const Vector3& scale)
{
	emitterName_ = "AreaEffect" + std::to_string(areaEffectCount_++);
	
	auto emitter = std::make_unique<ParticleEmitter>();
	emitter->Initialize(emitterName_);
	
	auto renderer = std::make_unique<SpriteRenderer>();
	renderer->Initialize("./Resources/gradation.png");
	renderer->SetBlendMode(BlendMode::Additive);
	emitter->SetRenderer(std::move(renderer));
	
	Vector3 adjustedScale = scale * Vector3(2.0f, 3.0f, 2.0f);
	
	emitter->AddModule(std::make_unique<SpawnRateModule>(2.0f));
	emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.5f, 0.0f, -0.5f), Vector3(0.5f, 0.5f, 0.5f)));
	emitter->AddModule(std::make_unique<InitialLifetimeModule>(0.5f, 0.7f));
	emitter->AddModule(std::make_unique<InitialScaleModule>(adjustedScale * 0.8f, adjustedScale * 1.2f));
	emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.5f, 0.8f, 1.0f, 0.8f)));
	emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(0.5f, 0.8f, 1.0f, 0.8f), Vector4(0.3f, 0.5f, 1.0f, 0.0f)));
	
	ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
}

void AreaEffect::Play(const Vector3& position)
{
	auto* emitter = ParticleManager::GetInstance()->GetEmitter(emitterName_);
	if (emitter)
	{
		emitter->SetPosition(position + Vector3(0.0f, 6.0f, 0.0f));
	}
}

void AreaEffect::Stop()
{
	auto* emitter = ParticleManager::GetInstance()->GetEmitter(emitterName_);
	if (emitter)
	{
		emitter->SetPosition(Vector3(0.0f, -1000.0f, 0.0f)); // 逕ｻ髱｢螟悶↓遘ｻ蜍・
	}
}

