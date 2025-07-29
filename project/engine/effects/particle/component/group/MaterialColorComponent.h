#pragma once
#include "effects/particle/ParticleGroup.h"
#include "effects/particle/component/interface/IParticleGroupComponent.h"
#include "math/Vector4.h"

class MaterialColorComponent : public IParticleGroupComponent
{
public:
    explicit MaterialColorComponent(const Vector4& color);

    void Update(ParticleGroup& group) override;

private:
    Vector4 color_;
};
