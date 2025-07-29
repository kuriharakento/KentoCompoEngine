#include "RandomInitialVelocityComponent.h"

RandomInitialVelocityComponent::RandomInitialVelocityComponent(const Vector3& minV, const Vector3& maxV)
    : minVelocity_(minV), maxVelocity_(maxV)
{
}

void RandomInitialVelocityComponent::Update(Particle& particle)
{
    if (!initialized_)
    {
        particle.velocity.x = RandomFloat(minVelocity_.x, maxVelocity_.x);
        particle.velocity.y = RandomFloat(minVelocity_.y, maxVelocity_.y);
        particle.velocity.z = RandomFloat(minVelocity_.z, maxVelocity_.z);
        initialized_ = true;
    }
}

float RandomInitialVelocityComponent::RandomFloat(float min, float max)
{
    float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    return min + r * (max - min);
}
