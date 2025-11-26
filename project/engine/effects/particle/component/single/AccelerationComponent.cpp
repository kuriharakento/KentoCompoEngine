#include "AccelerationComponent.h"

#include "base/GraphicsTypes.h"

AccelerationComponent::AccelerationComponent(const Vector3& accel)
    : acceleration_(accel)
{
}

void AccelerationComponent::Update(Particle& particle)
{
    // 加速度を速度に加算（v = v + a）
    particle.velocity += acceleration_;
}
