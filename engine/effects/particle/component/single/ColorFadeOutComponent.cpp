#include "ColorFadeOutComponent.h"

#include "base/GraphicsTypes.h"

void ColorFadeOutComponent::Update(Particle& particle)
{
    float lifeRatio = particle.currentTime / particle.lifeTime;
    if (lifeRatio > 1.0f) lifeRatio = 1.0f;
    particle.color.w = 1.0f - lifeRatio;
}
