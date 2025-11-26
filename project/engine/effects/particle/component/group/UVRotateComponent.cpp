#include "UVRotateComponent.h"

UVRotateComponent::UVRotateComponent(const Vector3& rotate)
    : rotate_(rotate)
{
}

void UVRotateComponent::Update(ParticleGroup& group)
{
    // 現在のUV回転値を取得
    Vector3 currentRotate = group.GetUVRotate();
    // 回転量を加算
    currentRotate += rotate_;
    // 新しいUV回転値を設定
    group.SetUVRotate(currentRotate);
}
