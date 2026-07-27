#pragma once
/**
 * @file SubEmitterModule.h
 * @brief サブエミッターモジュール
 * 
 * パーティクルイベント（生成、死亡、衝突、継続）時に
 * 子エフェクトを発生させる機能を提供。
 */
#include "effects/particle/module/IModule.h"
#include "effects/particle/module/ModulePriorities.h"
#include "effects/particle/ParticleEmitter.h"
#include "effects/particle/ParticleManager.h"
#include "effects/particle/ParticleEffect.h"
#include "effects/particle/serialization/ParticleEffectSerializer.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <random>

namespace KCE
{
/**
 * @brief サブエミッタートリガー条件
 */
enum class SubEmitterTrigger
{
	OnSpawn,      ///< パーティクル生成時に発生
	OnDeath,      ///< パーティクル死亡時に発生
	OnCollision,  ///< 衝突時に発生（CollisionModuleと連携）
	Continuous    ///< 継続的に一定レートで発生
};

/**
 * @brief サブエミッター設定
 */
struct SubEmitterConfig
{
	std::string effectPath;           ///< サブエフェクトのJSONファイルパス
	SubEmitterTrigger trigger = SubEmitterTrigger::OnDeath; ///< 発生トリガー条件
	
	// 継承設定
	bool inheritPosition = true;       ///< 親パーティクルの位置を継承
	bool inheritVelocity = false;      ///< 親パーティクルの速度を継承
	float inheritVelocityScale = 0.5f; ///< 速度継承時の倍率
	bool inheritColor = false;         ///< 親パーティクルの色を継承
	bool inheritScale = false;         ///< 親パーティクルのスケールを継承
	
	// 発生設定
	float probability = 1.0f;          ///< 発生確率（0.0～1.0）
	float continuousRate = 10.0f;      ///< Continuousモード時の1秒あたりの発生回数
};

/**
 * @brief サブエミッターモジュール
 * パーティクルイベント（生成、死亡、衝突等）時に子エフェクトを発生させる
 */
class SubEmitterModule : public IModule
{
public:
	SubEmitterModule() = default;

	/**
	 * @brief モジュール実行（このモジュールは特定イベントで呼ばれる）
	 * @param context パーティクルコンテキスト
	 */
	void Execute(ParticleContext& context) override
	{
		// このモジュールは通常のExecuteでは何もしない
		// RemoveDeadParticles等の特定イベント時にTriggerSubEmittersが呼ばれる
		(void)context;
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "SubEmitter"; }
	int32_t GetPriority() const override { return 200; }

	/**
	 * @brief サブエミッター設定を追加
	 * @param config サブエミッター設定
	 */
	void AddConfig(const SubEmitterConfig& config) { configs_.push_back(config); }

	/**
	 * @brief 全サブエミッター設定を取得
	 * @return サブエミッター設定リスト
	 */
	const std::vector<SubEmitterConfig>& GetConfigs() const { return configs_; }

	/**
	 * @brief 全サブエミッター設定を取得（編集可能）
	 * @return サブエミッター設定リスト
	 */
	std::vector<SubEmitterConfig>& GetConfigs() { return configs_; }

	/**
	 * @brief サブエミッター設定の数を取得
	 * @return 設定数
	 */
	size_t GetConfigCount() const { return configs_.size(); }

	/**
	 * @brief 指定インデックスの設定を取得
	 * @param index インデックス
	 * @return サブエミッター設定（無効なインデックスの場合nullptr）
	 */
	SubEmitterConfig* GetConfig(size_t index)
	{
		return index < configs_.size() ? &configs_[index] : nullptr;
	}

	/**
	 * @brief 指定インデックスの設定を取得（const版）
	 * @param index インデックス
	 * @return サブエミッター設定（無効なインデックスの場合nullptr）
	 */
	const SubEmitterConfig* GetConfig(size_t index) const
	{
		return index < configs_.size() ? &configs_[index] : nullptr;
	}

	/**
	 * @brief 指定インデックスの設定を削除
	 * @param index インデックス
	 */
	void RemoveConfig(size_t index)
	{
		if (index < configs_.size())
		{
			configs_.erase(configs_.begin() + static_cast<ptrdiff_t>(index));
		}
	}

	/**
	 * @brief トリガー条件に一致するサブエミッターをスポーン
	 * @param trigger トリガー条件
	 * @param particle 親パーティクル
	 * @param manager パーティクルマネージャー
	 */
	void TriggerSubEmitters(SubEmitterTrigger trigger, const Particle& particle, 
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
				std::uniform_real_distribution<float> dist(0.0f, 1.0f);
				if (dist(rng_) > config.probability) continue;
			}

			// サブエフェクトをロードして生成
			auto subEffect = ParticleEffectSerializer::Load(config.effectPath);
			if (!subEffect) continue;

			// 位置継承
			if (config.inheritPosition)
			{
				subEffect->SetPosition(particle.position);
			}

			// エフェクトを追加
			manager->AddEffect(std::move(subEffect));
		}
	}

	/**
	 * @brief Continuousモード用の更新
	 * @param deltaTime 経過時間（秒）
	 * @param particles パーティクルリスト
	 * @param manager パーティクルマネージャー
	 */
	void UpdateContinuous(float deltaTime, const std::vector<Particle>& particles,
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
						std::uniform_real_distribution<float> dist(0.0f, 1.0f);
						if (dist(rng_) > config.probability) continue;
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

private:
	std::vector<SubEmitterConfig> configs_;
	std::unordered_map<uint32_t, float> continuousAccumulators_;
	mutable std::mt19937 rng_{ std::random_device{}() };
};
} // namespace KCE
