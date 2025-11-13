#include "AreaEffect.h"
#include <effects/particle/component/single/ColorFadeOutComponent.h>
#include <effects/particle/component/group/UVTranslateComponent.h>

static uint32_t areaEffectCount = 0;  // 複数のエリアエフェクトを識別するためのカウンター

void AreaEffect::Initialize(const Vector3& rotate, const Vector3& scale)
{
	// ユニークなエフェクト名を生成
	areaEffectCount++;
	areaEmitter_ = std::make_unique<ParticleEmitter>();
	areaEmitter_->Initialize("AreaEffect" + std::to_string(areaEffectCount), "./Resources/gradation.png");
	areaEmitter_->SetEmitRange({}, {});
	areaEmitter_->SetInitialScale(scale * Vector3(2.0f,3.0f,2.0f));  // スケールを拡大して適用
	areaEmitter_->SetInitialRotation(rotate + Vector3(0.0f, 0.0f, 3.14f));
	areaEmitter_->SetBillborad(false);  // 固定方向で表示
	areaEmitter_->SetInitialLifeTime(0.6f);
	areaEmitter_->SetEmitRate(0.5f);
	areaEmitter_->SetModelType(ParticleGroup::ParticleType::Cube);
	
	// エフェクトコンポーネントの追加
	areaEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
	areaEmitter_->AddComponent(std::make_shared<UVTranslateComponent>(Vector3{ 0.1f, 0.0f, 0.0f }));  // UVスクロールで動きを表現
}

void AreaEffect::Play(const Vector3& position)
{
	// 少し上の位置でエフェクトを開始（視認性向上）
	areaEmitter_->Start(position + Vector3(0.0f, 6.0f, 0.0f), 10, 0.0f, true);
}