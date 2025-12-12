#pragma once
/**
 * @file SpawnModules.h
 * @brief パーティクルスポーンモジュール
 * 
 * レートベースの連続生成とバーストによる一括生成。
 */
#include "effects/particle/module/IModule.h"
#include "effects/particle/module/ModulePriorities.h"
#include "effects/particle/ParticleEmitter.h"
#include "math/Vector3.h"
#include "math/Vector4.h"

/**
 * @brief 基本スポーンモジュール（毎秒N個生成）
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
			particle.SetAlive(true);
			context.particles->push_back(particle);
			context.spawnCount++;
			timeSinceLastSpawn_ -= spawnInterval;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Spawn; }
	const char* GetName() const override { return "SpawnRate"; }
	int32_t GetPriority() const override { return 0; }

	void SetRate(float rate) { spawnRate_ = rate; }
	float GetRate() const { return spawnRate_; }

private:
	float spawnRate_ = 10.0f;
	float timeSinceLastSpawn_ = 0.0f;
};

/**
 * @brief バーストスポーンモジュール（一度に複数生成）
 */
class SpawnBurstModule : public IModule
{
public:
	SpawnBurstModule(uint32_t count = 10, float interval = 0.0f, int loops = 1)
		: burstCount_(count), burstInterval_(interval), loops_(loops) {}

	void Execute(ParticleContext& context) override
	{
		// 初回または繰り返し設定時
		if (!hasFired_ || (loops_ != 0 && burstInterval_ > 0.0f && (loops_ < 0 || currentLoop_ < loops_)))
		{
			timeSinceLastBurst_ += context.deltaTime;

			if (!hasFired_ || timeSinceLastBurst_ >= burstInterval_)
			{
				for (uint32_t i = 0; i < burstCount_; ++i)
				{
					Particle particle;
					particle.position = context.emitterPosition;
					particle.SetAlive(true);
					context.particles->push_back(particle);
					context.spawnCount++;
				}
				timeSinceLastBurst_ = 0.0f;
				hasFired_ = true;
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Spawn; }
	const char* GetName() const override { return "SpawnBurst"; }
	int32_t GetPriority() const override { return 0; }

	void SetCount(uint32_t count) { burstCount_ = count; }
	uint32_t GetCount() const { return burstCount_; }
	void SetDelay(float delay) { delay_ = delay; }
	float GetDelay() const { return delay_; }
	void SetInterval(float interval) { burstInterval_ = interval; }
	float GetInterval() const { return burstInterval_; }
	void SetLoops(int loops) { loops_ = loops; }
	int GetLoops() const { return loops_; }
	void Reset() { hasFired_ = false; timeSinceLastBurst_ = 0.0f; currentLoop_ = 0; }

private:
	uint32_t burstCount_ = 10;
	float delay_ = 0.0f;
	float burstInterval_ = 0.0f;
	float timeSinceLastBurst_ = 0.0f;
	int loops_ = 1;
	int currentLoop_ = 0;
	bool hasFired_ = false;
};
