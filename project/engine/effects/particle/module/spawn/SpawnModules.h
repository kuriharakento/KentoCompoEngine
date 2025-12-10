#pragma once
#include "effects/particle/module/IModule.h"
#include "effects/particle/ParticleEmitter.h"
#include "math/Vector3.h"
#include "math/Vector4.h"

/**
 * @brief 基本スポーンモジュール
 */
class SpawnRateModule : public IModule
{
public:
	SpawnRateModule(float rate = 10.0f) : spawnRate_(rate) {}

	void Execute(ParticleContext& context) override
	{
		timeSinceLastSpawn_ += context.deltaTime;
		float spawnInterval = 1.0f / spawnRate_;

		while (timeSinceLastSpawn_ >= spawnInterval)
		{
			Particle particle;
			particle.position = context.emitterPosition;
			context.particles->push_back(particle);
			context.spawnCount++;
			timeSinceLastSpawn_ -= spawnInterval;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::EmitterUpdate; }
	const char* GetName() const override { return "SpawnRate"; }

	void SetSpawnRate(float rate) { spawnRate_ = rate; }
	float GetSpawnRate() const { return spawnRate_; }

private:
	float spawnRate_ = 10.0f;
	float timeSinceLastSpawn_ = 0.0f;
};

/**
 * @brief バーストスポーンモジュール
 */
class SpawnBurstModule : public IModule
{
public:
	SpawnBurstModule(uint32_t count = 10, float interval = 1.0f)
		: burstCount_(count), burstInterval_(interval) {}

	void Execute(ParticleContext& context) override
	{
		timeSinceLastBurst_ += context.deltaTime;

		if (timeSinceLastBurst_ >= burstInterval_)
		{
			for (uint32_t i = 0; i < burstCount_; ++i)
			{
				Particle particle;
				particle.position = context.emitterPosition;
				context.particles->push_back(particle);
				context.spawnCount++;
			}
			timeSinceLastBurst_ = 0.0f;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::EmitterUpdate; }
	const char* GetName() const override { return "SpawnBurst"; }

	void SetBurstCount(uint32_t count) { burstCount_ = count; }
	void SetBurstInterval(float interval) { burstInterval_ = interval; }

private:
	uint32_t burstCount_ = 10;
	float burstInterval_ = 1.0f;
	float timeSinceLastBurst_ = 0.0f;
};
