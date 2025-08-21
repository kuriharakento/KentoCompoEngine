#pragma once
#include <vector>

#include "GameObjectInfo.h"

class EnemyManager;

class Wave
{
public:
	Wave(const std::vector<GameObjectInfo>& enemies);

	void Start(EnemyManager* enemyManager);

	bool IsStart() const { return isStart_; }
	bool IsClear() const { return isClear_; }

	void SetOnClearCallback(std::function<void()> callback) { onClearCallback_ = std::move(callback); }

private:
	std::vector<GameObjectInfo> enemies_;	//ウェーブ中にスポーンする敵の情報リスト

	bool isStart_ = false;	//ウェーブが開始されているかどうか
	bool isClear_ = false;	//ウェーブがクリアされているかどうか

	// クリア時のコールバック関数
	std::function<void()> onClearCallback_ = nullptr;
};

