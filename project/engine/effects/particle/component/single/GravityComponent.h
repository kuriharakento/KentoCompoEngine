#pragma once
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

class GravityComponent : public IParticleBehaviorComponent
{
public:
    explicit GravityComponent(const Vector3& g);
    void Update(Particle& particle) override;
private:
	Vector3 gravity; // 重力ベクトル
};
