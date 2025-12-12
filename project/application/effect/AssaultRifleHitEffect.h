#pragma once
#include <memory>
#include "math/Vector3.h"
#include "effects/particle/ParticleEmitter.h"

/**
 * @brief アサルトライフル命中エフェクト！EPS版！E
 */
class AssaultRifleHitEffect
{
public:
	void Initialize();
	void Play(const Vector3& position);

private:
	std::string emitterName_;
	static inline uint32_t effectCount_ = 0;
};

