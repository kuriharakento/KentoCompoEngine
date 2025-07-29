#pragma once
#include <cmath>
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

class OrbitComponent : public IParticleBehaviorComponent
{
public:
    OrbitComponent(const Vector3& c, float radius_, float speed);
	OrbitComponent(const Vector3* target, float radius_, float speed);
    void Update(Particle& particle) override;
private:
	const Vector3* target_ = nullptr; // 追従対象の位置
    Vector3 center_;
    float angularSpeed_;
    float radius_;
};
