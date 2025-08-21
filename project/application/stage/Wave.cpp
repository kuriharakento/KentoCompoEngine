#include "Wave.h"
#include "application/GameObject/combatable/character/enemy/EnemyManager.h"

Wave::Wave(const std::vector<GameObjectInfo>& enemies)
{
	enemies_ = enemies; // ウェーブ中にスポーンする敵の情報を設定
	isStart_ = false; // ウェーブは初期状態では開始されていない
	isClear_ = false; // ウェーブは初期状態ではクリアされていない
	onClearCallback_ = nullptr; // クリア時のコールバックは初期状態では設定されていない
}

void Wave::Start(EnemyManager* enemyManager)
{
	if (isStart_) { return; } // 既に開始されている場合は何もしない

	// 開始
	isStart_ = true;

	// クリア時のコールバック関数をエネミーマネージャーの敵全滅コールバックに設定
	enemyManager->SetOnAllEnemiesDefeatedCallback([this]() {
		if (!isClear_)
		{
			isClear_ = true;
			if (onClearCallback_)
			{
				// ウェーブクリア時のコールバックを呼び出す
				onClearCallback_(); 
				onClearCallback_ = nullptr;
			}
		}
												  });

	// 敵をエネミーマネージャーに追加
	enemyManager->AddEnemiesFromGameObjectInfo(enemies_);
}
