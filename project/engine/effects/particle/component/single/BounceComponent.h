#pragma once
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

class BounceComponent : public IParticleBehaviorComponent
{
public:
	explicit BounceComponent(float groundHeight, float restitution, float minVelocity);
	void Update(Particle& particle) override;
private:
	float groundHeight_; // 地面の高さ
	float restitution_; // 反発係数
	float minVelocity_; // 最小速度
};

