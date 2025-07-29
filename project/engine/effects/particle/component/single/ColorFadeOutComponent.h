#pragma once
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

class ColorFadeOutComponent : public IParticleBehaviorComponent
{
public:
    void Update(Particle& particle) override;
};
