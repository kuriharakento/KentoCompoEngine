#include "StageManager.h"

StageManager::StageManager()
{
}

StageManager::~StageManager()
{
	stageData_.reset();
	player_.reset();
	enemyManager_.reset();
	obstacleManager_.reset();
}

void StageManager::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager)
{
	object3dCommon_ = object3dCommon;
	lightManager_ = lightManager;
	// ステージデータの初期化
	stageData_ = std::make_unique<StageData>();

	// --- ゲームオブジェクトの初期化 --- //
	// プレイヤー
	player_ = std::make_unique<Player>("Player");
	// 敵マネージャー
	enemyManager_ = std::make_unique<EnemyManager>();
	// 障害物マネージャー
	obstacleManager_ = std::make_unique<ObstacleManager>();
}

void StageManager::Update()
{
	// プレイヤーの更新
	player_->Update();
	// 敵マネージャーの更新
	enemyManager_->Update();
	// 障害物の更新
	obstacleManager_->Update();
}

void StageManager::Draw(CameraManager* camera)
{
	// プレイヤーの描画
	player_->Draw(camera);
	// 敵マネージャーの描画
	enemyManager_->Draw(camera);
	// 障害物の描画
	obstacleManager_->Draw(camera);
}

void StageManager::DrawImGui()
{
}

void StageManager::LoadStage(const std::string& stageName)
{
	// フルパスを作成
	std::string fullpath = "stage/" + stageName + ".json";
	// ステージデータをロード
	stageData_->LoadJson(fullpath);
	// ゲームオブジェクトを生成
	CreateGameObjects();
}

void StageManager::CreateGameObjects()
{
	// ステージデータからゲームオブジェクトを生成
	for (const auto& objInfo : stageData_->gameObjects)
	{
		if (objInfo.disabled) continue; // 無効化されているオブジェクトはスキップ
		// ゲームオブジェクトの生成
		// プレイヤーや敵などの特定のタイプに応じて追加処理
		if (objInfo.type == "PlayerSpawn")
		{
			player_->Initialize(object3dCommon_, lightManager_);
			player_->SetModel(/*objInfo.name*/ "cube.obj");
			player_->SetPosition(objInfo.transform.translate);
			player_->SetRotation(objInfo.transform.rotate);
			player_->SetScale(objInfo.transform.scale);
			player_->Update();
		}
		/*else if (objInfo.type == "Enemy")
		{
			enemyManager_->AddEnemy(gameObject);
		}*/
		else if (objInfo.type == "Obstacle")
		{
			auto obstacle = std::make_unique<Obstacle>(objInfo.name);
			obstacle->Initialize(object3dCommon_, lightManager_);
			obstacle->SetModel(/*objInfo.name*/"cube.obj");
			obstacle->SetPosition(objInfo.transform.translate);
			obstacle->SetRotation(objInfo.transform.rotate);
			obstacle->SetScale(objInfo.transform.scale);
			obstacleManager_->AddObstacle(std::move(obstacle));
		}

	}
}

