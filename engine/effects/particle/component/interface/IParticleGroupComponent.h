#pragma once
#include "IParticleComponent.h"

class ParticleGroup;

class IParticleGroupComponent : virtual  public IParticleComponent
{
public:
	virtual ~IParticleGroupComponent() = default;
    virtual void Update(ParticleGroup& group) = 0;
};
