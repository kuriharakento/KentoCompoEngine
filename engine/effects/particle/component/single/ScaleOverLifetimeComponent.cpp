#include "ScaleOverLifetimeComponent.h"

ScaleOverLifetimeComponent::ScaleOverLifetimeComponent(float start, float end)
    : startScale_(start), endScale_(end)
{
}

void ScaleOverLifetimeComponent::Update(Particle& particle)
{
    float lifeRatio = particle.currentTime / particle.lifeTime;
    if (lifeRatio > 1.0f) lifeRatio = 1.0f;
    float scale = startScale_ + (endScale_ - startScale_) * lifeRatio;
    particle.transform.scale = Vector3(scale, scale, scale);
}
