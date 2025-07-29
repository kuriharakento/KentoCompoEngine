#include "AssaultRifleHitEffect.h"

// component
#include "effects/particle/component/group/UVTranslateComponent.h"
#include "effects/particle/component/single/ColorFadeOutComponent.h"
#include "effects/particle/component/single/RotationComponent.h"
#include "effects/particle/component/single/ScaleOverLifetimeComponent.h"

static uint32_t effectCount = 0; // エフェクトの識別子として使用

void AssaultRifleHitEffect::Initialize()
{
	impactEmitter_ = std::make_unique<ParticleEmitter>();
	impactEmitter_->Initialize("AssaultRifleHitEffect" + std::to_string(effectCount), "./Resources/gradationLine.png");
	effectCount++;
	impactEmitter_->SetEmitRange({}, {});
	impactEmitter_->SetInitialLifeTime(0.4f);
	impactEmitter_->SetBillborad(true);
	impactEmitter_->SetRandomRotation(true);
	impactEmitter_->SetRandomRotationRange(AABB{ Vector3{ -3.14f, 3.14f, 0.0f }, Vector3{ 3.14f, 3.14f, 0.0f } });
	impactEmitter_->SetModelType(ParticleGroup::ParticleType::Ring); // 円環状のパーティクル

	//=======コンポーネントの追加=========================
	// 回転コンポーネント
	impactEmitter_->AddComponent(std::make_shared<RotationComponent>(Vector3(0.05f,0.03f,0.0f)));
	// スケール変化コンポーネント
	impactEmitter_->AddComponent(std::make_shared<ScaleOverLifetimeComponent>(0.0f, 3.0f));
	// 色フェードアウトコンポーネント
	impactEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
	// UVトランスレート
	impactEmitter_->AddComponent(std::make_shared<UVTranslateComponent>(Vector3{ 0.1f, 0.0f, 0.0f })); // UVを毎フレーム大きくずらす
}

void AssaultRifleHitEffect::Play(const Vector3& position)
{
	// エミッターを開始
	impactEmitter_->Start(position, 5, 0.0f, false); // 1回だけ放出
}
