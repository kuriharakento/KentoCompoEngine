#include "ParticleTestScene.h"

#include <numbers>

#include "effects/particle/ParticleManager.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/update/UpdateModules.h"
#include "manager/graphics/LineManager.h"
#include "manager/scene/CameraManager.h"
#include "math/VectorColorCodes.h"
#include "scene/manager/SceneManager.h"

void ParticleTestScene::Initialize()
{
	StartState(SceneState::Playing);

	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());
	debugCamera_->Start();

	sceneManager_->GetCameraManager()->GetActiveCamera()->SetTranslate({ 0.0f, 5.0f, -20.0f });
	sceneManager_->GetCameraManager()->GetActiveCamera()->SetRotate({ 0.0f, 0.0f, 0.0f });

	// 新パーティクルシステムでオーラエフェクトを作成
	{
		auto emitter = std::make_unique<ParticleEmitter>();
		emitter->Initialize("auraCylinder");
		
		auto renderer = std::make_unique<SpriteRenderer>();
		renderer->Initialize("./Resources/gradation.png");
		renderer->SetBlendMode(BlendMode::Additive);
		emitter->SetRenderer(std::move(renderer));
		
		emitter->AddModule(std::make_unique<SpawnRateModule>(1.0f));
		emitter->AddModule(std::make_unique<InitialLifetimeModule>(2.0f, 2.0f));
		emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(0.3f, 10.0f, 0.3f), Vector3(0.3f, 10.0f, 0.3f)));
		emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(1.0f, 1.0f, 1.0f, 1.0f)));
		
		ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
	}

	{
		auto emitter = std::make_unique<ParticleEmitter>();
		emitter->Initialize("auraMist");
		
		auto renderer = std::make_unique<SpriteRenderer>();
		renderer->Initialize("./Resources/gradation.png");
		renderer->SetBlendMode(BlendMode::Additive);
		emitter->SetRenderer(std::move(renderer));
		
		emitter->AddModule(std::make_unique<SpawnRateModule>(5.0f));
		emitter->AddModule(std::make_unique<InitialPositionModule>(Vector3(-0.5f, 0.0f, -0.5f), Vector3(0.5f, 0.0f, 0.5f)));
		emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3(0.0f, 0.3f, 0.0f), Vector3(0.0f, 0.5f, 0.0f)));
		emitter->AddModule(std::make_unique<InitialLifetimeModule>(1.0f, 2.0f));
		emitter->AddModule(std::make_unique<InitialScaleModule>(Vector3(1.0f, 2.0f, 1.0f), Vector3(1.5f, 3.0f, 1.5f)));
		emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(1.0f, 1.0f, 1.0f, 0.3f)));
		emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(1.0f, 1.0f, 1.0f, 0.3f), Vector4(1.0f, 1.0f, 1.0f, 0.0f)));
		
		ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
	}

	{
		auto emitter = std::make_unique<ParticleEmitter>();
		emitter->Initialize("auraFloor");
		
		auto renderer = std::make_unique<SpriteRenderer>();
		renderer->Initialize("./Resources/gradation.png");
		renderer->SetBlendMode(BlendMode::Additive);
		emitter->SetRenderer(std::move(renderer));
		
		emitter->AddModule(std::make_unique<SpawnRateModule>(1.0f));
		emitter->AddModule(std::make_unique<InitialLifetimeModule>(1.0f, 1.0f));
		emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(1.0f, 1.0f, 1.0f, 1.0f)));
		emitter->AddModule(std::make_unique<ScaleOverLifetimeModule>(Vector3(0.0f, 0.0f, 0.0f), Vector3(3.0f, 3.0f, 3.0f)));
		emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(1.0f, 1.0f, 1.0f, 0.0f)));
		
		ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
	}
}

void ParticleTestScene::Finalize()
{
}

// ==================================================
// 状態フック
// ==================================================

void ParticleTestScene::OnEnterPlaying()
{
	auto* cylinder = ParticleManager::GetInstance()->GetEmitter("auraCylinder");
	auto* mist = ParticleManager::GetInstance()->GetEmitter("auraMist");
	auto* floor = ParticleManager::GetInstance()->GetEmitter("auraFloor");

	if (cylinder) cylinder->SetPosition(Vector3(0.0f, 10.0f, 0.0f));
	if (mist) mist->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
	if (floor) floor->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
}

void ParticleTestScene::OnUpdatePlaying()
{
	if (debugCamera_) debugCamera_->Update();
}

void ParticleTestScene::Draw2D()
{
}

void ParticleTestScene::Draw3D()
{
	LineManager::GetInstance()->DrawGrid(
		30.0f,
		5.0f,
		VectorColorCodes::White
	);
}