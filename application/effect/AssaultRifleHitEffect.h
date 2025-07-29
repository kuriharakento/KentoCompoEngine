#pragma once
// effects
#include "effects/particle/ParticleEmitter.h"

class AssaultRifleHitEffect
{
public:
	void Initialize();

	void Play(const Vector3& position);

private:
	// エミッター
	std::unique_ptr<ParticleEmitter> impactEmitter_;
};

