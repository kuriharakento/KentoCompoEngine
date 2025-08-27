#pragma once

// system
#include "graphics/3d/Object3dCommon.h"
#include "manager/scene/CameraManager.h"
#include "manager/scene/LightManager.h"

// app
#include "Stage.h"
#include "StageData.h"
#include "application/GameObject/Combatable/character/enemy/EnemyManager.h"
#include "application/GameObject/Combatable/character/player/Player.h"
#include "application/GameObject/obstacle/ObstacleManager.h"

/**
 * \brief ステージ管理クラス。
 * ステージの情報をJsonから読み込み、ゲームオブジェクトの初期化、更新、描画を行う。
 */
class StageManager
{
public:
	StageManager();
	~StageManager();

	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, CameraManager* camera);
	void Update();
	void Draw();
	void DrawImGui();

	// ステージデータを読み込み
	void LoadStage(const std::string& stageName);
	// ステージデータをもとに各ゲームオブジェクトの情報を分ける
	void CreateInfosFromStageData();

	bool IsStageCleared() const { return stage_ ? stage_->IsCleared() : false; }

	// ゲームオブジェクト取得
	Player* GetPlayer() const { return player_.get(); }
	EnemyManager* GetEnemyManager() const { return enemyManager_.get(); }
	ObstacleManager* GetObstacleManager() const { return obstacleManager_.get(); }
	Stage* GetStage() const { return stage_.get(); }

private:
	Object3dCommon* object3dCommon_; // 3Dオブジェクトの共通情報
	LightManager* lightManager_; // ライトマネージャー
	CameraManager* cameraManager_; // カメラマネージャー

	std::shared_ptr<StageData> stageData_; // ステージデータ

	// -------- ゲームオブジェクト -------- //

	// プレイヤー
	std::unique_ptr<Player> player_;
	// 敵マネージャー
	std::unique_ptr<EnemyManager> enemyManager_;
	// 障害物マネージャー
	std::unique_ptr<ObstacleManager> obstacleManager_;
	// ステージ
	std::unique_ptr<Stage> stage_;
};

