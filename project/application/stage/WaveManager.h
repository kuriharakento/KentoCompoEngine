#pragma once
#include <vector>

#include "Wave.h"
class EnemyManager;

class WaveManager
{
public:
	WaveManager(EnemyManager* enemyManager, const std::vector<Wave>& waves);

	void Update();
	void Start();

	// クリアした時のコールバック設定
	void SetOnAllWavesCleared(std::function<void()> callback) { onAllWavesCleared_ = std::move(callback); }

	// クリアしたか
	bool IsAllCleared() const { return currentWaveIndex_ >= static_cast<int>(waves_.size()); }

	// Waveを強制スキップする例
	void SkipToNextWave();

private:
	void StartCurrentWave();

private:
	EnemyManager* enemyManager_; // 敵マネージャーへのポインタ
	std::vector<Wave> waves_; // ウェーブのリスト
	int currentWaveIndex_;
	std::function<void()> onAllWavesCleared_; // すべてのウェーブクリア時のコールバック

	// ウェーブ間ウェイト用
	bool waitForNextWave_;
	float waitTimer_;
};

