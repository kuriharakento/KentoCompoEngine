#include "StageManager.h"

#include "manager/editor/JsonEditorManager.h"

StageManager::StageManager()
{
}

StageManager::~StageManager()
{
	// 各ゲームオブジェクトを明示的に解放
	stageData_.reset();
	player_.reset();
	enemyManager_.reset();
	obstacleManager_.reset();
	stage_.reset();
}

#include "engine/manager/effect/PostProcessManager.h"

void StageManager::Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, LightManager* lightManager, CameraManager* camera, ShadowMapManager* shadowMapManager, PostProcessManager* postProcessManager)
{
	object3dCommon_ = object3dCommon;
	spriteCommon_ = spriteCommon;
	lightManager_ = lightManager;
	cameraManager_ = camera;
	shadowMapManager_ = shadowMapManager;
	postProcessManager_ = postProcessManager;

	// ステージデータの初期化
	stageData_ = std::make_unique<StageData>();

	// 障害物データの初期化
	obstacleData_ = std::make_shared<ObstacleData>();

	// デバッグエディターに登録（実行時編集を可能にする）
	JsonEditorManager::GetInstance()->Register("stageData", stageData_);
	JsonEditorManager::GetInstance()->Register("obstacleData", obstacleData_);

	// --- 各マネージャーの初期化 --- //

	// 敵マネージャーの初期化
	enemyManager_ = std::make_unique<EnemyManager>();
	enemyManager_->Initialize(object3dCommon_, spriteCommon, camera, lightManager, shadowMapManager, nullptr); // ターゲットは後で設定
	enemyManager_->SetCameraManager(cameraManager_);

	// 障害物マネージャーの初期化
	obstacleManager_ = std::make_unique<ObstacleManager>();
	obstacleManager_->Initialize(object3dCommon_, lightManager);
}

void StageManager::Update()
{
	// デバッグUIの更新
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

	// ステージの更新（エリア・ウェーブ進行）
	stage_->Update(cameraManager_);

#ifdef _DEBUG
	// デバッグモード：ステージデータと障害物データを同期
	// JSONエディターでの編集内容を即座に反映するため
	std::vector< GameObjectInfo> obstacleInfos;
	for (const auto& data : stageData_->gameObjects)
	{
		if (data.type == "Obstacle" || data.type == "BarrierBlock" || data.type == "Floor")
		{
			obstacleInfos.push_back(data);
		}
	}
	obstacleData_->SetObstacles(obstacleInfos);
#endif
}

void StageManager::UpdateTransforms(CameraManager* camera)
{
	// プレイヤーの行列更新
	if (player_)
	{
		player_->UpdateTransform(camera);
	}

	// 敵マネージャーの行列更新
	if (enemyManager_)
	{
		enemyManager_->UpdateTransform(cameraManager_);
	}

	// 障害物マネージャーの行列更新
	if (obstacleManager_)
	{
		obstacleManager_->UpdateTransforms(cameraManager_);
	}
}

void StageManager::Draw3D()
{
	// プレイヤーの描画
	if (player_)
	{
		player_->Draw3D(cameraManager_);
	}

	// 敵マネージャーの通常描画（非ECS）
	if (enemyManager_)
	{
		enemyManager_->DrawStandard3D(cameraManager_);
	}

	// 障害物の描画
	if (obstacleManager_)
	{
		obstacleManager_->Draw(cameraManager_);
	}

	// 敵マネージャーのECSインスタンス描画（ルートシグネチャが変わるため最後に実行）
	if (enemyManager_)
	{
		enemyManager_->DrawInstanced3D(cameraManager_);
	}
}

void StageManager::DrawShadow()
{
	// プレイヤーのシャドウ描画
	if (player_)
	{
		player_->DrawShadow();
	}

	// 敵マネージャーのシャドウ描画
	if (enemyManager_)
	{
		enemyManager_->DrawShadow();
	}

	// 障害物のシャドウ描画
	if (obstacleManager_)
	{
		obstacleManager_->DrawShadow();
	}
}

void StageManager::Draw2D()
{
	// プレイヤーの2D描画
	if (player_)
	{
		player_->Draw2D();
	}
	// 敵マネージャーの2D描画
	if (enemyManager_)
	{
		enemyManager_->Draw2D();
	}
}

void StageManager::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("Stage Manager");

	// 敵を全クリアするボタン（デバッグ用）
	if(ImGui::Button("Clear Enemies"))
	{
		if (enemyManager_)
		{
			enemyManager_->Clear();
		}
	}

	// 障害物を全クリアするボタン（デバッグ用）
	if (ImGui::Button("Clear Obstacles"))
	{
		if (obstacleManager_)
		{
			obstacleManager_->Clear();
		}
	}

	// ステージをロードするボタン（デバッグ用）
	if (ImGui::Button("Load Stage"))
	{
		LoadStage("field"); // サンプルステージをロード
	}

	ImGui::End();
	
#endif
}

void StageManager::LoadStage(const std::string& stageName)
{
	// ステージファイルのパスを構築
	std::string dirpath = "stage/" + stageName;
	std::string json = ".json";
	std::string areaJson = "_area" + json;

	// 既存のゲームオブジェクトをクリア
	player_.reset();              // プレイヤーは1体のみなのでリセット
	enemyManager_->Clear();       // 敵を全削除
	obstacleManager_->Clear();    // 障害物と床を全削除

	// ステージデータ（固定オブジェクト配置）をロード
	stageData_->LoadJson(dirpath + json);

	// ステージデータから各ゲームオブジェクトを生成
	CreateInfosFromStageData();

	// ステージ（エリア・ウェーブ管理）を初期化
	stage_ = std::make_unique<Stage>(
		object3dCommon_,
		lightManager_,
		enemyManager_.get(),
		dirpath + areaJson  // エリア・ウェーブ定義ファイル
	);

	// ステージを開始
	stage_->Start();
}

void StageManager::CreateInfosFromStageData()
{
	// 障害物情報を格納するリスト
	std::vector<GameObjectInfo> obstacleInfos;

	// ステージデータの各オブジェクトをタイプごとに分類
#ifdef _DEBUG
	OutputDebugStringA(("stageData_ gameObjects count: " + std::to_string(stageData_->gameObjects.size()) + "\n").c_str());
	for (const auto& obj : stageData_->gameObjects)
	{
		OutputDebugStringA(("  type=" + obj.type + " name=" + obj.name + "\n").c_str());
	}
#endif
	for(const auto& objInfo : stageData_->gameObjects)
	{
		// 無効化されているオブジェクトはスキップ
		if (objInfo.disabled) continue;

		// タイプに応じて処理を分岐
		if (objInfo.type == "PlayerSpawn")
		{
			// プレイヤーのスポーン処理
			// ゲーム中に1体のみなのでここで生成
			if (!player_)
			{
				player_ = std::make_unique<Player>();
			}
			// 敵マネージャーにプレイヤーをターゲットとして設定
			enemyManager_->SetTarget(player_.get());

			// プレイヤーの初期化と配置
			player_->Initialize(object3dCommon_, spriteCommon_, lightManager_, enemyManager_.get(), cameraManager_, postProcessManager_);
			player_->SetModel("player");
			player_->SetPosition(objInfo.transform.translate);
			player_->SetRotation(objInfo.transform.rotate);
			player_->SetScale(objInfo.transform.scale);
		}
		else if (objInfo.type == "EnemySpawn")
		{
			// 敵のスポーン処理
			// 注: 現在は未使用（敵はウェーブ管理システムで生成される）
		}
		else if (objInfo.type == "Obstacle" || objInfo.type == "BarrierBlock" || objInfo.type == "Floor")
		{
			// 障害物・床の情報を収集（Floorはコライダーなしで生成される）
			obstacleInfos.push_back(objInfo);
		}
	}

	// 障害物データを設定
	obstacleData_->SetObstacles(obstacleInfos);
	obstacleManager_->SetObstacleData(obstacleData_.get());
}
