#include "AreaEffect.h"
#include "effects/particle/ParticleManager.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/update/UpdateModules.h"

namespace
{
	// スケール調整係数
	constexpr float kScaleMultiplierX = 2.0f;
	constexpr float kScaleMultiplierY = 3.0f;
	constexpr float kScaleMultiplierZ = 2.0f;
	constexpr float kScaleVariationMin = 0.8f;
	constexpr float kScaleVariationMax = 1.2f;

	// パーティクル生成設定
	constexpr float kSpawnRate = 2.0f;

	// 位置範囲
	constexpr float kPositionRangeMin = -0.5f;
	constexpr float kPositionRangeMaxX = 0.5f;
	constexpr float kPositionRangeMaxY = 0.5f;
	constexpr float kPositionRangeMaxZ = 0.5f;
	constexpr float kPositionMinY = 0.0f;

	// ライフタイム設定
	constexpr float kLifetimeMin = 0.5f;
	constexpr float kLifetimeMax = 0.7f;

	// エフェクトのY軸オフセット
	constexpr float kEffectHeightOffset = 6.0f;

	// 停止時の移動先Y座標
	constexpr float kHiddenPositionY = -1000.0f;
}

void AreaEffect::Initialize(const Vector3& rotate, const Vector3& scale)
{
	// ユニークなエミッター名を生成
	emitterName_ = "AreaEffect" + std::to_string(areaEffectCount_++);
	
	// エミッターの作成と初期化
	auto emitter = std::make_unique<ParticleEmitter>();
	emitter->Initialize(emitterName_);
	
	// レンダラーの設定（加算合成で光る効果）
	auto renderer = std::make_unique<SpriteRenderer>();
	renderer->Initialize("./Resources/gradation.png");
	renderer->SetBlendMode(BlendMode::Additive);
	emitter->SetRenderer(std::move(renderer));
	
	// スケールの調整
	Vector3 adjustedScale = scale * Vector3(kScaleMultiplierX, kScaleMultiplierY, kScaleMultiplierZ);
	
	// パーティクル生成モジュールの追加
	emitter->AddModule(std::make_unique<SpawnRateModule>(kSpawnRate));
	emitter->AddModule(std::make_unique<InitialPositionModule>(
		Vector3(kPositionRangeMin, kPositionMinY, kPositionRangeMin),
		Vector3(kPositionRangeMaxX, kPositionRangeMaxY, kPositionRangeMaxZ)));
	emitter->AddModule(std::make_unique<InitialLifetimeModule>(kLifetimeMin, kLifetimeMax));
	emitter->AddModule(std::make_unique<InitialScaleModule>(adjustedScale * kScaleVariationMin, adjustedScale * kScaleVariationMax));
	emitter->AddModule(std::make_unique<InitialColorModule>(Vector4(0.5f, 0.8f, 1.0f, 0.8f)));
	emitter->AddModule(std::make_unique<ColorFadeModule>(Vector4(0.5f, 0.8f, 1.0f, 0.8f), Vector4(0.3f, 0.5f, 1.0f, 0.0f)));
	
	// エミッターをマネージャーに登録
	ParticleManager::GetInstance()->AddEmitter(std::move(emitter));
}

void AreaEffect::Play(const Vector3& position)
{
	// エミッターを取得して位置を設定
	auto* emitter = ParticleManager::GetInstance()->GetEmitter(emitterName_);
	if (emitter)
	{
		// 高さオフセットを加えた位置に設定
		emitter->SetPosition(position + Vector3(0.0f, kEffectHeightOffset, 0.0f));
	}
}

void AreaEffect::Stop()
{
	// エミッターを取得して画面外に移動
	auto* emitter = ParticleManager::GetInstance()->GetEmitter(emitterName_);
	if (emitter)
	{
		emitter->SetPosition(Vector3(0.0f, kHiddenPositionY, 0.0f));
	}
}

