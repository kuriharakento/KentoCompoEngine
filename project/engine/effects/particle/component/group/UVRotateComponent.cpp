#include "UVRotateComponent.h"

UVRotateComponent::UVRotateComponent(const Vector3& rotate)
    : rotate_(rotate)
{
}

void UVRotateComponent::Update(ParticleGroup& group)
{
    Vector3 currentRotate = group.GetUVRotate();
    currentRotate += rotate_;
    group.SetUVRotate(currentRotate);
}
