#include "UVTranslateComponent.h"

#include "time/TimeManager.h"

namespace
{
	// UV座標のラップ閾値（0.0〜1.0の範囲で繰り返す）
	constexpr float kUVWrapThreshold = 1.0f;
}

UVTranslateComponent::UVTranslateComponent(const Vector3& translate)
    : translate_(translate)
{
}

void UVTranslateComponent::Update(ParticleGroup& group)
{
	// デルタタイムを取得
	float delta = TimeManager::GetInstance().GetGameContext().deltaTime;
	// 現在のUV平行移動値を取得
    Vector3 currentTranslate = group.GetUVTranslate();
	// 移動量を時間に基づいて加算
    currentTranslate += translate_ * delta;

	// UV座標を0〜1の範囲でラップ（X成分）
	if (currentTranslate.x > kUVWrapThreshold) currentTranslate.x -= kUVWrapThreshold;
	if (currentTranslate.x < 0.0f) currentTranslate.x += kUVWrapThreshold;

	// UV座標を0〜1の範囲でラップ（Y成分）
	if (currentTranslate.y > kUVWrapThreshold) currentTranslate.y -= kUVWrapThreshold;
	if (currentTranslate.y < 0.0f) currentTranslate.y += kUVWrapThreshold;

	// 新しいUV平行移動値を設定
    group.SetUVTranslate(currentTranslate);
}
