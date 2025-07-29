#include "DragComponent.h"

#include "base/GraphicsTypes.h"

DragComponent::DragComponent(float drag)
    : dragFactor_(drag)
{
}

void DragComponent::Update(Particle& particle)
{
    particle.velocity *= dragFactor_;
}
