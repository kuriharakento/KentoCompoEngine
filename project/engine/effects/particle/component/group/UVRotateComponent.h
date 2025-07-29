#pragma once
#include "effects/particle/ParticleGroup.h"
#include "effects/particle/component/interface/IParticleGroupComponent.h"

class UVRotateComponent : public IParticleGroupComponent
{
public:
    explicit UVRotateComponent(const Vector3& rotate);

    void Update(ParticleGroup& group) override;

private:
    Vector3 rotate_;
};
