#pragma once
#include "effects/particle/ParticleGroup.h"
#include "effects/particle/component/interface/IParticleGroupComponent.h"
#include "math/Vector3.h"

class UVTranslateComponent : public IParticleGroupComponent
{
public:
    explicit UVTranslateComponent(const Vector3& translate);

    void Update(ParticleGroup& group) override;

private:
    Vector3 translate_;
};
