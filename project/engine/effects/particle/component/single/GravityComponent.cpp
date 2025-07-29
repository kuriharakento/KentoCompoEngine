#include "GravityComponent.h"

#include "base/GraphicsTypes.h"

GravityComponent::GravityComponent(const Vector3& g)
    : gravity(g)
{
}

void GravityComponent::Update(Particle& particle)
{
    particle.velocity += gravity;
}