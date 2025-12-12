#include "SubEmitterModule.h"
#include "effects/particle/ParticleManager.h"
#include "effects/particle/ParticleEffect.h"
#include "effects/particle/serialization/ParticleEffectSerializer.h"
#include <random>

void SubEmitterModule::TriggerSubEmitters(SubEmitterTrigger trigger, const Particle& particle,
                                          ParticleManager* manager)
{
	if (!manager) return;

	for (const auto& config : configs_)
	{
		if (config.trigger != trigger) continue;
		if (config.effectPath.empty()) continue;

		// 確率チェック
		if (config.probability < 1.0f)
		{
			static std::random_device rd;
			static std::mt19937 gen(rd());
			std::uniform_real_distribution<float> dist(0.0f, 1.0f);
			if (dist(gen) > config.probability) continue;
		}

		// サブエフェクトをロードして生成
		auto subEffect = ParticleEffectSerializer::Load(config.effectPath);
		if (!subEffect) continue;

		// 位置継承
		if (config.inheritPosition)
		{
			subEffect->SetPosition(particle.position);
		}

		// 速度継承（各エミッターのパーティクルに対して）
		// Note: 速度継承はエフェクトレベルでは難しいので、
		// ParticleManagerに登録する際に考慮する必要がある
		// 現在の実装では位置のみ

		manager->AddEffect(std::move(subEffect));
	}
}

void SubEmitterModule::UpdateContinuous(float deltaTime, const std::vector<Particle>& particles,
                                        ParticleManager* manager)
{
	if (!manager) return;

	for (const auto& particle : particles)
	{
		if (!particle.IsAlive()) continue;

		for (const auto& config : configs_)
		{
			if (config.trigger != SubEmitterTrigger::Continuous) continue;
			if (config.effectPath.empty()) continue;

			// アキュムレータ更新
			float& accum = continuousAccumulators_[particle.id];
			accum += deltaTime;

			float interval = 1.0f / config.continuousRate;
			while (accum >= interval)
			{
				accum -= interval;

				// 確率チェック
				if (config.probability < 1.0f)
				{
					static std::random_device rd;
					static std::mt19937 gen(rd());
					std::uniform_real_distribution<float> dist(0.0f, 1.0f);
					if (dist(gen) > config.probability) continue;
				}

				// サブエフェクトをロードして生成
				auto subEffect = ParticleEffectSerializer::Load(config.effectPath);
				if (!subEffect) continue;

				if (config.inheritPosition)
				{
					subEffect->SetPosition(particle.position);
				}

				manager->AddEffect(std::move(subEffect));
			}
		}
	}

	// 死んだパーティクルのアキュムレータを削除
	for (auto it = continuousAccumulators_.begin(); it != continuousAccumulators_.end();)
	{
		bool found = false;
		for (const auto& p : particles)
		{
			if (p.id == it->first && p.IsAlive())
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			it = continuousAccumulators_.erase(it);
		}
		else
		{
			++it;
		}
	}
}
