#pragma once
#include "effects/particle/ParticleEmitter.h"

class AreaEffect
{
public:
	void Initialize(const Vector3& rotate, const Vector3& scale);

	void Play(const Vector3& position);
	void Stop()
	{
		areaEmitter_->StopEmit();
	}

private:
	// エミッター
	std::unique_ptr<ParticleEmitter> areaEmitter_;
};

