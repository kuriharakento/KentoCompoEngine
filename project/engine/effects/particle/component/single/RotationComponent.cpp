#include "RotationComponent.h"

RotationComponent::RotationComponent(const Vector3& rotSpeed)
    : rotationSpeed_(rotSpeed)
{
}

void RotationComponent::Update(Particle& particle)
{
    // 回転速度を回転角度に加算
    particle.transform.rotate += rotationSpeed_;
}
