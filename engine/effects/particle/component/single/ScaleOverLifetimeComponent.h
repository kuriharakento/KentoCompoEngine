#pragma once
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

class ScaleOverLifetimeComponent : public IParticleBehaviorComponent
{
public:
    ScaleOverLifetimeComponent(float start, float end);
    void Update(Particle& particle) override;
private:
    float startScale_;
    float endScale_;
};
