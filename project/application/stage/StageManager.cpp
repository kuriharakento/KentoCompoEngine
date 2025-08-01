#include "StageManager.h"

#include "manager/editor/JsonEditorManager.h"

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
	JsonEditorManager::GetInstance()->Register("stageData", stageData_);

	// --- マネージャーの初期化 --- //
	// 敵マネージャー
	enemyManager_ = std::make_unique<EnemyManager>();
	enemyManager_->Initialize(object3dCommon_, lightManager, nullptr); // ターゲットは後で設定
	// 障害物マネージャー
	obstacleManager_ = std::make_unique<ObstacleManager>();
	obstacleManager_->Initialize(object3dCommon_, lightManager);
}

void StageManager::Update()
{
	DrawImGui();
	// プレイヤーの更新
	if (player_)
	{
		player_->Update();
	}
	// 敵マネージャーの更新
	if (enemyManager_)
	{
		enemyManager_->Update();
	}
	
	// 障害物の更新
	if (obstacleManager_)
	{
		obstacleManager_->Update();
	}
}

void StageManager::Draw(CameraManager* camera)
{
	// プレイヤーの描画
	if (player_)
	{
		player_->Draw(camera);
	}
	// 敵マネージャーの描画
	if (enemyManager_)
	{
		enemyManager_->Draw(camera);
	}
	// 障害物の描画
	if (obstacleManager_)
	{
		obstacleManager_->Draw(camera);
	}
}

void StageManager::DrawImGui()
{
#ifdef _DEBUG
	static std::string fileName = "object";
	static char fileNameBuffer[256] = "object"; // バッファを用意

	ImGui::Begin("Stage Manager");
	if (ImGui::InputText("Stage Name", fileNameBuffer, sizeof(fileNameBuffer)))
	{
		// 入力があったら std::string に反映
		fileName = fileNameBuffer;
	}
	if (ImGui::Button("Load Stage"))
	{
		// ステージのロード
		LoadStage(fileName); // 例として "object" ステージをロード
	}
	ImGui::End();
#endif
}

void StageManager::LoadStage(const std::string& stageName)
{
	// フルパスを作成
	std::string fullpath = "stage/" + stageName + ".json";
	// ステージデータをロード
	stageData_->LoadJson(fullpath);
	// ステージデータからゲームオブジェクトの情報を生成
	CreateInfosFromStageData();
}

void StageManager::CreateInfosFromStageData()
{
	// 各マネージャーに渡すデータ
	std::vector<GameObjectInfo> enemyInfos;
	std::vector<GameObjectInfo> obstacleInfos;

	for(const auto& objInfo : stageData_->gameObjects)
	{
		if (objInfo.disabled) continue; // 無効化されているオブジェクトはスキップ
		// プレイヤーや敵などの特定のタイプに応じて追加処理
		if (objInfo.type == "PlayerSpawn")
		{
			// プレイヤーはゲーム中に１っ体だけなのでここで生成
			if (!player_)
			{
				player_ = std::make_unique<Player>("Player");
				enemyManager_->SetTarget(player_.get());
			}
			player_->Initialize(object3dCommon_, lightManager_);
			player_->SetModel(/*objInfo.name*/ "cube.obj");
			player_->SetPosition(objInfo.transform.translate);
			player_->SetRotation(objInfo.transform.rotate);
			player_->SetScale(objInfo.transform.scale);
		}
		else if (objInfo.type == "EnemySpawn")
		{
			// 敵の情報を敵マネージャーに追加
			enemyInfos.push_back(objInfo);
		}
		else if (objInfo.type == "Obstacle")
		{
			// 障害物の情報を障害物マネージャーに追加
			obstacleInfos.push_back(objInfo);
		}
	}

	// 各マネージャーにデータを渡す
	obstacleManager_->SetObstacleData(obstacleInfos);
	enemyManager_->SetEnemyData(enemyInfos);
}
