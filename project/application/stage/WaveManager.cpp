#include "WaveManager.h"

#include "time/TimeManager.h"

WaveManager::WaveManager(EnemyManager* enemyManager, const std::vector<Wave>& waves)
{
	enemyManager_ = enemyManager; // 敵マネージャーのポインタを設定
	waves_ = waves; // ウェーブのリストを設定
	currentWaveIndex_ = 0; // 現在のウェーブインデックスを初期化
	onAllWavesCleared_ = nullptr; // すべてのウェーブクリア時のコールバックを初期化
	waitForNextWave_ = false; // 次のウェーブを待つフラグを初期化
	waitTimer_ = 0.0f; // ウェイトタイマーを初期化
}

void WaveManager::Update()
{
	// ウェーブ間ウェイト演出処理例
	if (waitForNextWave_)
	{
		waitTimer_ -= TimeManager::GetInstance().GetDeltaTime();
		if (waitTimer_ <= 0.0f)
		{
			waitForNextWave_ = false;
			StartCurrentWave();
		}
	}

}

void WaveManager::SkipToNextWave()
{
	waitForNextWave_ = false;
	waitTimer_ = 0.0f;
	++currentWaveIndex_;
	StartCurrentWave();
}

void WaveManager::StartCurrentWave()
{
	if (currentWaveIndex_ >= waves_.size())
	{
		if (onAllWavesCleared_) onAllWavesCleared_();
		return;
	}
	waves_[currentWaveIndex_].SetOnClearCallback([this]() {
		waitForNextWave_ = true; // 次のウェーブを待つフラグを立てる
		waitTimer_ = 2.0f; // ウェイトタイマーを設定（例: 2秒）
		++currentWaveIndex_;
		StartCurrentWave();
												 });
	waves_[currentWaveIndex_].Start(enemyManager_);
}

void WaveManager::Start()
{
	if (waves_.empty()) { return; } // ウェーブがない場合は何もしない

	StartCurrentWave(); // 現在のウェーブを開始
}

