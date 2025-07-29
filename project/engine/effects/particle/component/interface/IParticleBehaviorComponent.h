#pragma once
#include "IParticleComponent.h"
#include "base/GraphicsTypes.h"

class IParticleBehaviorComponent : virtual public IParticleComponent
{
public:
	virtual ~IParticleBehaviorComponent() = default;
	virtual void Update(Particle& particle) = 0;
};
