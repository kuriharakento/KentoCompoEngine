#include "UVScaleComponent.h"

UVScaleComponent::UVScaleComponent(const Vector3& scale)
    : scale_(scale)
{
}

void UVScaleComponent::Update(ParticleGroup& group)
{
    // 現在のUVスケール値を取得
    Vector3 currentScale = group.GetUVScale();
    // スケール変化量を加算
    currentScale += scale_;
    // 新しいUVスケール値を設定
    group.SetUVScale(currentScale);
}
