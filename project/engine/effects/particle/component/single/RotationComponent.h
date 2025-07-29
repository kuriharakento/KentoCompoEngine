#pragma once
#include "base/GraphicsTypes.h"
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

class RotationComponent : public IParticleBehaviorComponent
{
public:
    explicit RotationComponent(const Vector3& rotSpeed);
    void Update(Particle& particle) override;
private:
    Vector3 rotationSpeed_;
};
