#pragma once
/**
 * @file RibbonModules.h
 * @brief リボン専用モジュール
 * 
 * リボンパーティクルの生成時補間など、
 * リボンレンダー専用の処理を提供。
 */
#include "effects/particle/module/IModule.h"
#include "effects/particle/module/ModulePriorities.h"
#include "effects/particle/ParticleEmitter.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include <cmath>
#include <algorithm>
#include <unordered_map>

/**
 * @brief リボン補間モジュール（生成時補間）
 * 
 * パーティクル生成時に、前のパーティクルとの距離が離れている場合
 * Catmull-Rom補間で中間パーティクルを追加する。
 * これは生成時のみ行われ、形状は固定される。
 */
class RibbonInterpolationModule : public IModule
{
public:
	RibbonInterpolationModule(float maxDistance = 0.1f) : maxDistance_(maxDistance) {}

	void Execute(ParticleContext& context) override
	{
		if (!context.particles) return;

		auto& particles = *context.particles;
		if (particles.empty()) return;

		// 新しく生成されたパーティクル（age == 0）を処理
		std::vector<Particle> newParticles;
		
		for (auto& particle : particles)
		{
			// 新しく生成されたパーティクルのみ処理
			if (particle.age > 0.0f || !particle.IsAlive()) continue;

			uint32_t ribbonId = particle.ribbonId > 0 ? particle.ribbonId : 1;
			
			// このリボンIDの履歴を取得
			auto& history = particleHistory_[ribbonId];
			
			if (history.size() >= 2)
			{
				// Catmull-Rom補間用に4点を取得
				const Particle& p0 = history.size() >= 3 ? history[history.size() - 3] : history[history.size() - 2];
				const Particle& p1 = history.size() >= 2 ? history[history.size() - 2] : history[history.size() - 1];
				const Particle& p2 = history.back();
				const Particle& p3 = particle;

				// p2とp3の距離を計算
				float dx = p3.position.x - p2.position.x;
				float dy = p3.position.y - p2.position.y;
				float dz = p3.position.z - p2.position.z;
				float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

				// 距離が閾値を超えていたら補間
				if (distance > maxDistance_)
				{
					int numInterpolations = static_cast<int>(std::ceil(distance / maxDistance_)) - 1;
					
					for (int j = 1; j <= numInterpolations; ++j)
					{
						float t = static_cast<float>(j) / static_cast<float>(numInterpolations + 1);
						
						Particle interp = InterpolateCatmullRom(p0, p1, p2, p3, t);
						interp.ribbonId = ribbonId;
						interp.SetAlive(true);
						newParticles.push_back(interp);
					}
				}
			}
			else if (history.size() == 1)
			{
				// 2点しかない場合は線形補間
				const Particle& p1 = history.back();
				const Particle& p2 = particle;

				float dx = p2.position.x - p1.position.x;
				float dy = p2.position.y - p1.position.y;
				float dz = p2.position.z - p1.position.z;
				float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

				if (distance > maxDistance_)
				{
					int numInterpolations = static_cast<int>(std::ceil(distance / maxDistance_)) - 1;
					
					for (int j = 1; j <= numInterpolations; ++j)
					{
						float t = static_cast<float>(j) / static_cast<float>(numInterpolations + 1);
						
						Particle interp = InterpolateLinear(p1, p2, t);
						interp.ribbonId = ribbonId;
						interp.SetAlive(true);
						newParticles.push_back(interp);
					}
				}
			}

			// 履歴に追加（最大4つまで保持）
			history.push_back(particle);
			if (history.size() > 4)
			{
				history.erase(history.begin());
			}
		}

		// 補間パーティクルを追加
		for (auto& p : newParticles)
		{
			particles.push_back(p);
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Spawn; }
	const char* GetName() const override { return "RibbonInterpolation"; }
	int32_t GetPriority() const override { return 100; }  // 他のSpawnモジュールの後に実行

	void SetMaxDistance(float distance) { maxDistance_ = distance; }
	float GetMaxDistance() const { return maxDistance_; }

	// 履歴をクリア（エフェクト再開時など）
	void ClearHistory() { particleHistory_.clear(); }

private:
	float maxDistance_ = 0.1f;
	std::unordered_map<uint32_t, std::vector<Particle>> particleHistory_;

	// Catmull-Rom補間
	static float CatmullRom(float p0, float p1, float p2, float p3, float t)
	{
		float t2 = t * t;
		float t3 = t2 * t;
		return 0.5f * (
			(2.0f * p1) +
			(-p0 + p2) * t +
			(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
			(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
		);
	}

	Particle InterpolateCatmullRom(const Particle& p0, const Particle& p1, 
		const Particle& p2, const Particle& p3, float t)
	{
		Particle result;
		result.position.x = CatmullRom(p0.position.x, p1.position.x, p2.position.x, p3.position.x, t);
		result.position.y = CatmullRom(p0.position.y, p1.position.y, p2.position.y, p3.position.y, t);
		result.position.z = CatmullRom(p0.position.z, p1.position.z, p2.position.z, p3.position.z, t);
		
		// 他のプロパティは線形補間（p2とp3の間）
		result.velocity.x = p2.velocity.x + (p3.velocity.x - p2.velocity.x) * t;
		result.velocity.y = p2.velocity.y + (p3.velocity.y - p2.velocity.y) * t;
		result.velocity.z = p2.velocity.z + (p3.velocity.z - p2.velocity.z) * t;
		
		result.scale.x = p2.scale.x + (p3.scale.x - p2.scale.x) * t;
		result.scale.y = p2.scale.y + (p3.scale.y - p2.scale.y) * t;
		result.scale.z = p2.scale.z + (p3.scale.z - p2.scale.z) * t;
		
		result.color.x = p2.color.x + (p3.color.x - p2.color.x) * t;
		result.color.y = p2.color.y + (p3.color.y - p2.color.y) * t;
		result.color.z = p2.color.z + (p3.color.z - p2.color.z) * t;
		result.color.w = p2.color.w + (p3.color.w - p2.color.w) * t;
		
		result.age = p2.age + (p3.age - p2.age) * t;
		result.lifetime = p2.lifetime + (p3.lifetime - p2.lifetime) * t;
		
		return result;
	}

	Particle InterpolateLinear(const Particle& p1, const Particle& p2, float t)
	{
		Particle result;
		result.position.x = p1.position.x + (p2.position.x - p1.position.x) * t;
		result.position.y = p1.position.y + (p2.position.y - p1.position.y) * t;
		result.position.z = p1.position.z + (p2.position.z - p1.position.z) * t;
		
		result.velocity.x = p1.velocity.x + (p2.velocity.x - p1.velocity.x) * t;
		result.velocity.y = p1.velocity.y + (p2.velocity.y - p1.velocity.y) * t;
		result.velocity.z = p1.velocity.z + (p2.velocity.z - p1.velocity.z) * t;
		
		result.scale.x = p1.scale.x + (p2.scale.x - p1.scale.x) * t;
		result.scale.y = p1.scale.y + (p2.scale.y - p1.scale.y) * t;
		result.scale.z = p1.scale.z + (p2.scale.z - p1.scale.z) * t;
		
		result.color.x = p1.color.x + (p2.color.x - p1.color.x) * t;
		result.color.y = p1.color.y + (p2.color.y - p1.color.y) * t;
		result.color.z = p1.color.z + (p2.color.z - p1.color.z) * t;
		result.color.w = p1.color.w + (p2.color.w - p1.color.w) * t;
		
		result.age = p1.age + (p2.age - p1.age) * t;
		result.lifetime = p1.lifetime + (p2.lifetime - p1.lifetime) * t;
		
		return result;
	}
};
