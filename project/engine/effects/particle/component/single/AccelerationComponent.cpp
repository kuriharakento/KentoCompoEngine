#include "AccelerationComponent.h"

#include "base/GraphicsTypes.h"

AccelerationComponent::AccelerationComponent(const Vector3& accel)
    : acceleration_(accel)
{
}

void AccelerationComponent::Update(Particle& particle)
{
    particle.velocity += acceleration_;
}
