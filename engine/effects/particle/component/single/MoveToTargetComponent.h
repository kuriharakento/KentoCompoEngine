#pragma once
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

class MoveToTargetComponent : public IParticleBehaviorComponent
{
public:
	MoveToTargetComponent(const Vector3& target, const float& speed);
	MoveToTargetComponent(const Vector3* target, const float& speed);
	void Update(Particle& particle) override;

private:
	Vector3 target_; // 移動先の座標
	const Vector3* targetPtr_ = nullptr; // 移動先の座標ポインタ
	float speed_ = 0.0f; // 移動速度
};

