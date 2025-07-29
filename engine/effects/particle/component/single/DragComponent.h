#pragma once
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

class DragComponent : public IParticleBehaviorComponent
{
public:
    explicit DragComponent(float drag);
    void Update(Particle& particle) override;
private:
    float dragFactor_;
};
