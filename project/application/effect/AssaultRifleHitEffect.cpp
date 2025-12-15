#include "AssaultRifleHitEffect.h"
#include "effects/particle/ParticleManager.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/update/UpdateModules.h"

namespace
{
	// バースト設定
	constexpr int kBurstParticleCount = 5;
	constexpr float kBurstDuration = 0.5f;

	// 位置範囲
	constexpr float kPositionRangeMin = -0.1f;
	constexpr float kPositionRangeMax = 0.1f;

	// ライフタイム設定
	constexpr float kLifetimeMin = 0.3f;
	constexpr float kLifetimeMax = 0.5f;

	// 初期スケール範囲
	constexpr float kInitialScaleMin = 0.1f;
	constexpr float kInitialScaleMax = 0.3f;

	// スケール変化範囲
	constexpr float kFinalScaleValue = 3.0f;
}

void AssaultRifleHitEffect::Initialize()
{
	// ユニークなエミッター名を生成
	emitterName_ = "AssaultRifleHitEffect" + std::to_string(effectCount_++);
	
	// エミッターの作成と初期化
	auto emitter = std::make_unique<ParticleEmitter>();
	emitter->Initialize(emitterName_);
	
	// レンダラーの設定（加算合成で光る効果）
	auto renderer = std::make_unique<SpriteRenderer>();
	renderer->Initialize("./Resources/gradationLine.png");
	renderer->SetBlendMode(BlendMode::Additive);
	emitter->SetRenderer(std::move(renderer));
	
	// バースト生成モジュールの追加
	emitter->AddModule(std::make_unique<SpawnBurstModule>(kBurstParticleCount, kBurstDuration));
	emitter->AddModule(std::make_unique<InitialPositionModule>(
		Vector3(kPositionRangeMin, kPositionRangeMin, kPositionRangeMin),
		Vector3(kPositionRangeMax, kPositionRangeMax, kPositionRangeMax)));
	emitter->AddModule(std::make_unique<InitialLifetimeModule>(kLifetimeMin, kLifetimeMax));
	emitter->AddModule(std::make_unique<InitialScaleModule>(
		Vector3(kInitialScaleMin, kInitialScaleMin, kInitialScaleMin),
		Vector3(kInitialScaleMax, kInitialScaleMax, kInitialScaleMax)));
	
	// 色設定（オレンジ色の命中エフェクト）
	emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(1.0f, 0.8f, 0.3f, 1.0f)));
	emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(1.0f, 0.8f, 0.3f, 1.0f), Vector4(1.0f, 0.5f, 0.0f, 0.0f)));
	
	// 時間経過でスケールを拡大
	emitter->AddModule(std::make_unique<ScaleOverLifetimeModule>(
		Vector3(kInitialScaleMin, kInitialScaleMin, kInitialScaleMin),
		Vector3(kFinalScaleValue, kFinalScaleValue, kFinalScaleValue)));
	
	// エミッターをマネージャーに登録
	ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
}

void AssaultRifleHitEffect::Play(const Vector3& position)
{
	// エミッターを取得して位置を設定
	auto* emitter = ParticleManager::GetInstance()->GetEmitter(emitterName_);
	if (emitter)
	{
		emitter->SetPosition(position);
	}
}


