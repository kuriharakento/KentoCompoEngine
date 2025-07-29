#include "UVScaleComponent.h"

UVScaleComponent::UVScaleComponent(const Vector3& scale)
    : scale_(scale)
{
}

void UVScaleComponent::Update(ParticleGroup& group)
{
    Vector3 currentScale = group.GetUVScale();
    currentScale += scale_;
    group.SetUVScale(currentScale);
}
