#include "GravityComponent.h"

#include "base/GraphicsTypes.h"

GravityComponent::GravityComponent(const Vector3& g)
    : gravity(g)
{
}

void GravityComponent::Update(Particle& particle)
{
    // 重力を速度に加算（落下や引力の影響をシミュレート）
    particle.velocity += gravity;
}