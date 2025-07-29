#pragma once
#include "effects/particle/ParticleGroup.h"
#include "effects/particle/component/interface/IParticleGroupComponent.h"
#include "math/Vector3.h"

class UVScaleComponent : public IParticleGroupComponent
{
public:
    explicit UVScaleComponent(const Vector3& scale);

    void Update(ParticleGroup& group) override;

private:
    Vector3 scale_;
};
