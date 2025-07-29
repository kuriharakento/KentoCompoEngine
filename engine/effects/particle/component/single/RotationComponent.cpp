#include "RotationComponent.h"

RotationComponent::RotationComponent(const Vector3& rotSpeed)
    : rotationSpeed_(rotSpeed)
{
}

void RotationComponent::Update(Particle& particle)
{
    particle.transform.rotate += rotationSpeed_;
}
