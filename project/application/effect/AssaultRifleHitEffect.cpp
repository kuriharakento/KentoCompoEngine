#include "AssaultRifleHitEffect.h"

// component
#include "effects/particle/component/group/UVTranslateComponent.h"
#include "effects/particle/component/single/ColorFadeOutComponent.h"
#include "effects/particle/component/single/RotationComponent.h"
#include "effects/particle/component/single/ScaleOverLifetimeComponent.h"
#include "AreaEffect.h"

static uint32_t effectCount = 0;  // 複数のエフェクトを識別するためのカウンター

void AssaultRifleHitEffect::Initialize()
{
	// ユニークなエフェクト名を生成
	impactEmitter_ = std::make_unique<ParticleEmitter>();
	impactEmitter_->Initialize("AssaultRifleHitEffect" + std::to_string(effectCount), "./Resources/gradationLine.png");
	effectCount++;
	
	// リング型パーティクルの基本設定
	impactEmitter_->SetEmitRange({}, {});
	impactEmitter_->SetInitialLifeTime(0.4f);
	impactEmitter_->SetBillborad(true);
	impactEmitter_->SetRandomRotation(true);
	impactEmitter_->SetRandomRotationRange(AABB{ Vector3{ -3.14f, 3.14f, 0.0f }, Vector3{ 3.14f, 3.14f, 0.0f } });
	impactEmitter_->SetModelType(ParticleGroup::ParticleType::Ring);

	// エフェクトコンポーネントの追加（回転、拡大、フェード、UVスクロール）
	impactEmitter_->AddComponent(std::make_shared<RotationComponent>(Vector3(0.05f,0.03f,0.0f)));
	impactEmitter_->AddComponent(std::make_shared<ScaleOverLifetimeComponent>(0.0f, 3.0f));  // 小さい状態から急速に拡大
	impactEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
	impactEmitter_->AddComponent(std::make_shared<UVTranslateComponent>(Vector3{ 0.1f, 0.0f, 0.0f }));
}

void AssaultRifleHitEffect::Play(const Vector3& position)
{
	// 一度だけ複数のリングを放出（バースト型）
	impactEmitter_->Start(position, 5, 0.0f, false);
}
