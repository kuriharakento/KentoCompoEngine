#pragma once
/**
 * @file TrailModule.h
 * @brief トレイルパーティクル生成モジュール
 *
 * 親パーティクルに追従するトレイルパーティクルを生成する。
 * TrailRendererと組み合わせて使用することで、
 * Niagaraライクなトレイルエフェクトを実現。
 */
#include "effects/particle/module/IModule.h"
#include "effects/particle/ParticleEmitter.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include <unordered_map>
#include <vector>
#include <cmath>
#include <algorithm>

/**
 * @brief 幅カーブ用キーポイント
 */
struct TrailWidthKey
{
	float time = 0.0f;   // 0.0(先頭) ~ 1.0(末尾)
	float width = 1.0f;  // 幅係数
};

/**
 * @brief トレイルモジュール
 *
 * 親パーティクルの移動に応じてトレイルパーティクルを生成する。
 * 主にSpawnフェーズで動作し、親パーティクルごとにトレイルを管理。
 */
class TrailModule : public IModule
{
public:
	TrailModule() = default;

	void Execute(ParticleContext& context) override
	{
		if (!context.particles) return;

		auto& particles = *context.particles;
		if (particles.empty()) return;

		float deltaTime = context.deltaTime;

		// 生成用の新しいパーティクルリスト
		std::vector<Particle> newTrailParticles;

		for (auto& particle : particles)
		{
			if (!particle.IsAlive()) continue;

			uint32_t parentId = particle.id;
			auto& history = particleHistories_[parentId];

			// 時間アキュムレータを更新
			history.timeSinceLastSpawn += deltaTime;

			// 生成間隔をチェック
			float spawnInterval = 1.0f / trailRate_;
			if (history.timeSinceLastSpawn < spawnInterval && !history.positions.empty())
			{
				continue;
			}

			// 距離チェック
			if (!history.positions.empty())
			{
				const Vector3& lastPos = history.positions.back();
				float dx = particle.position.x - lastPos.x;
				float dy = particle.position.y - lastPos.y;
				float dz = particle.position.z - lastPos.z;
				float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

				if (distance < minDistance_)
				{
					continue;
				}
			}

			// トレイルパーティクルを生成
			if (!history.positions.empty())
			{
				Particle trailParticle;
				trailParticle.position = particle.position;
				trailParticle.velocity = { 0.0f, 0.0f, 0.0f }; // トレイルは移動しない

				// スケール
				float widthScale = EvaluateWidthCurve(0.0f);
				trailParticle.scale = { 
					particle.scale.x * widthScale * trailWidth_, 
					particle.scale.y * widthScale * trailWidth_, 
					particle.scale.z 
				};

				// 色の継承
				if (inheritColor_)
				{
					trailParticle.color = particle.color;
					trailParticle.initialColor = particle.initialColor;
				}
				else
				{
					trailParticle.color = trailColor_;
					trailParticle.initialColor = trailColor_;
				}

				// 寿命
				trailParticle.lifetime = trailLifetime_;
				trailParticle.age = 0.0f;

				// リボンID（トレイルのグループ化用）
				trailParticle.ribbonId = parentId;
				trailParticle.ribbonWidth = trailWidth_ * particle.scale.x;

				// フラグ設定
				trailParticle.SetAlive(true);
				trailParticle.SetRibbonHead(history.positions.empty());

				newTrailParticles.push_back(trailParticle);
			}

			// 履歴を更新
			history.positions.push_back(particle.position);
			history.timeSinceLastSpawn = 0.0f;

			// 最大履歴数を制限
			while (history.positions.size() > maxHistorySize_)
			{
				history.positions.erase(history.positions.begin());
			}
		}

		// 新しいトレイルパーティクルを追加
		for (auto& trailParticle : newTrailParticles)
		{
			particles.push_back(trailParticle);
		}

		// 死んだパーティクルの履歴を削除
		CleanupDeadParticles(particles);
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "Trail"; }
	int32_t GetPriority() const override { return 50; }  // 物理更新の後に実行

	//===== 設定 =====//

	/** @brief トレイル生成レート（個/秒） */
	void SetTrailRate(float rate) { trailRate_ = rate; }
	float GetTrailRate() const { return trailRate_; }

	/** @brief トレイルの寿命 */
	void SetTrailLifetime(float lifetime) { trailLifetime_ = lifetime; }
	float GetTrailLifetime() const { return trailLifetime_; }

	/** @brief トレイルの幅 */
	void SetTrailWidth(float width) { trailWidth_ = width; }
	float GetTrailWidth() const { return trailWidth_; }

	/** @brief 最小生成距離 */
	void SetMinDistance(float distance) { minDistance_ = distance; }
	float GetMinDistance() const { return minDistance_; }

	/** @brief 親の色を継承するか */
	void SetInheritColor(bool inherit) { inheritColor_ = inherit; }
	bool GetInheritColor() const { return inheritColor_; }

	/** @brief トレイルの色（inheritColor=false時） */
	void SetTrailColor(const Vector4& color) { trailColor_ = color; }
	const Vector4& GetTrailColor() const { return trailColor_; }

	/** @brief 幅カーブを設定 */
	void SetWidthCurve(const std::vector<TrailWidthKey>& curve) { widthCurve_ = curve; }
	const std::vector<TrailWidthKey>& GetWidthCurve() const { return widthCurve_; }

	/** @brief 履歴をクリア */
	void ClearHistory() { particleHistories_.clear(); }

private:
	/**
	 * @brief 幅カーブを評価
	 * @param t 正規化された時間 (0.0 ~ 1.0)
	 */
	float EvaluateWidthCurve(float t) const
	{
		if (widthCurve_.empty())
		{
			return 1.0f;
		}

		if (widthCurve_.size() == 1)
		{
			return widthCurve_[0].width;
		}

		// 補間
		for (size_t i = 0; i < widthCurve_.size() - 1; ++i)
		{
			if (t >= widthCurve_[i].time && t <= widthCurve_[i + 1].time)
			{
				float localT = (t - widthCurve_[i].time) / 
				               (widthCurve_[i + 1].time - widthCurve_[i].time);
				return widthCurve_[i].width + 
				       (widthCurve_[i + 1].width - widthCurve_[i].width) * localT;
			}
		}

		return widthCurve_.back().width;
	}

	/**
	 * @brief 死んだパーティクルの履歴を削除
	 */
	void CleanupDeadParticles(const std::vector<Particle>& particles)
	{
		std::unordered_map<uint32_t, bool> aliveIds;
		for (const auto& p : particles)
		{
			if (p.IsAlive())
			{
				aliveIds[p.id] = true;
			}
		}

		for (auto it = particleHistories_.begin(); it != particleHistories_.end();)
		{
			if (aliveIds.find(it->first) == aliveIds.end())
			{
				it = particleHistories_.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

private:
	/**
	 * @brief パーティクルごとの位置履歴
	 */
	struct ParticleHistory
	{
		std::vector<Vector3> positions;
		float timeSinceLastSpawn = 0.0f;
	};

	std::unordered_map<uint32_t, ParticleHistory> particleHistories_;

	//===== 設定値 =====//
	float trailRate_ = 60.0f;           // 1秒あたりの生成数
	float trailLifetime_ = 1.0f;        // トレイルの寿命
	float trailWidth_ = 0.5f;           // トレイルの幅
	float minDistance_ = 0.05f;         // 最小生成距離
	bool inheritColor_ = true;          // 親の色を継承
	Vector4 trailColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	std::vector<TrailWidthKey> widthCurve_;
	size_t maxHistorySize_ = 100;
};
