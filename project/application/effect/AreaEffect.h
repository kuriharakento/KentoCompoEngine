#pragma once
#include <memory>
#include "math/Vector3.h"
#include "effects/particle/ParticleEmitter.h"

/**
 * @brief エリア表示エフェクト！EPS版！E
 */
class AreaEffect
{
public:
	void Initialize(const Vector3& rotate, const Vector3& scale);
	void Play(const Vector3& position);
	void Stop();

private:
	std::string emitterName_;
	static inline uint32_t areaEffectCount_ = 0;
};

