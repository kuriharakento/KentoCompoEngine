#include "AreaEffect.h"
#include <effects/particle/component/single/ColorFadeOutComponent.h>
#include <effects/particle/component/group/UVTranslateComponent.h>

static uint32_t areaEffectCount = 0; // エフェクトの識別子として使用

void AreaEffect::Initialize(const Vector3& rotate, const Vector3& scale)
{
	areaEffectCount++;
	areaEmitter_ = std::make_unique<ParticleEmitter>();
	areaEmitter_->Initialize("AreaEffect" + std::to_string(areaEffectCount), "./Resources/gradation.png");
	areaEmitter_->SetEmitRange({}, {});
	areaEmitter_->SetInitialScale(scale * Vector3(2.0f,3.0f,2.0f));
	areaEmitter_->SetInitialRotation(rotate + Vector3(0.0f, 0.0f, 3.14f));
	areaEmitter_->SetBillborad(false);
	areaEmitter_->SetInitialLifeTime(0.6f);
	areaEmitter_->SetEmitRate(0.5f);
	areaEmitter_->SetModelType(ParticleGroup::ParticleType::Cube);
	//=======コンポーネントの追加=========================
	// 色フェードアウトコンポーネント
	areaEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
	// UVトランスレート
	areaEmitter_->AddComponent(std::make_shared<UVTranslateComponent>(Vector3{ 0.1f, 0.0f, 0.0f })); // UVを毎フレーム大きくずらす
}

void AreaEffect::Play(const Vector3& position)
{
	// エミッターを開始
	areaEmitter_->Start(position + Vector3(0.0f, 6.0f, 0.0f), 10, 0.0f, true);
}