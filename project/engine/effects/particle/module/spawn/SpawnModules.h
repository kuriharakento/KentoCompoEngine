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
	static constexpr float kDefaultSpawnRate = 10.0f;
	
	/**
	 * @brief コンストラクタ
	 * @param rate スポーンレート（毎秒のパーティクル数）
	 */
	SpawnRateModule(float rate = kDefaultSpawnRate) : spawnRate_(rate) {}

	/**
	 * @brief モジュールを実行
	 * @param context パーティクルコンテキスト
	 */
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

	/**
	 * @brief スポーンレートを設定
	 * @param rate スポーンレート（毎秒のパーティクル数）
	 */
	void SetRate(float rate) { spawnRate_ = rate; }
	
	/**
	 * @brief スポーンレートを取得
	 * @return スポーンレート
	 */
	float GetRate() const { return spawnRate_; }

	/**
	 * @brief 内部状態をリセット
	 */
	void Reset() override
	{
		timeSinceLastSpawn_ = 0.0f;
	}

private:
	float spawnRate_ = kDefaultSpawnRate;
	float timeSinceLastSpawn_ = 0.0f;
};

/**
 * @brief バーストスポーンモジュール（一度に複数生成）
 */
class SpawnBurstModule : public IModule
{
public:
	static constexpr uint32_t kDefaultBurstCount = 10;
	static constexpr int kInfiniteLoops = -1;
	
	/**
	 * @brief コンストラクタ
	 * @param count バースト時のパーティクル数
	 * @param interval バースト間隔（秒）
	 * @param loops ループ回数（-1で無限）
	 */
	SpawnBurstModule(uint32_t count = kDefaultBurstCount, float interval = 0.0f, int loops = 1)
		: burstCount_(count), burstInterval_(interval), loops_(loops) {}

	/**
	 * @brief モジュールを実行
	 * @param context パーティクルコンテキスト
	 */
	void Execute(ParticleContext& context) override
	{
		timeSinceLastBurst_ += context.deltaTime;

		// 初回バーストの遅延チェック
		if (!hasFired_ && timeSinceLastBurst_ < delay_)
		{
			return;
		}

		// ループ終了チェック
		if (hasFired_ && loops_ >= 0 && currentLoop_ >= loops_)
		{
			return;
		}

		// バースト間隔チェック（初回 or インターバル経過）
		bool shouldBurst = !hasFired_ || 
			(burstInterval_ > 0.0f && timeSinceLastBurst_ >= burstInterval_);

		if (shouldBurst)
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
			currentLoop_++;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Spawn; }
	const char* GetName() const override { return "SpawnBurst"; }
	int32_t GetPriority() const override { return 0; }

	/**
	 * @brief バースト時のパーティクル数を設定
	 * @param count パーティクル数
	 */
	void SetCount(uint32_t count) { burstCount_ = count; }
	
	/**
	 * @brief バースト時のパーティクル数を取得
	 * @return パーティクル数
	 */
	uint32_t GetCount() const { return burstCount_; }
	
	/**
	 * @brief 初回バーストの遅延時間を設定
	 * @param delay 遅延時間（秒）
	 */
	void SetDelay(float delay) { delay_ = delay; }
	
	/**
	 * @brief 初回バーストの遅延時間を取得
	 * @return 遅延時間（秒）
	 */
	float GetDelay() const { return delay_; }
	
	/**
	 * @brief バースト間隔を設定
	 * @param interval 間隔（秒）
	 */
	void SetInterval(float interval) { burstInterval_ = interval; }
	
	/**
	 * @brief バースト間隔を取得
	 * @return 間隔（秒）
	 */
	float GetInterval() const { return burstInterval_; }
	
	/**
	 * @brief ループ回数を設定
	 * @param loops ループ回数（-1で無限）
	 */
	void SetLoops(int loops) { loops_ = loops; }
	
	/**
	 * @brief ループ回数を取得
	 * @return ループ回数
	 */
	int GetLoops() const { return loops_; }
	
	/**
	 * @brief 状態をリセット
	 */
	void Reset() override { hasFired_ = false; timeSinceLastBurst_ = 0.0f; currentLoop_ = 0; }

private:
	uint32_t burstCount_ = kDefaultBurstCount;
	float delay_ = 0.0f;
	float burstInterval_ = 0.0f;
	float timeSinceLastBurst_ = 0.0f;
	int loops_ = 1;
	int currentLoop_ = 0;
	bool hasFired_ = false;
};
