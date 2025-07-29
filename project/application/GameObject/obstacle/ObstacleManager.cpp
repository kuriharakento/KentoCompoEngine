#include "ObstacleManager.h"

#include "application/GameObject/component/collision/OBBColliderComponent.h"
#include "manager/editor/JsonEditorManager.h"

void ObstacleManager::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager)
{
	// ポインタをメンバ変数に記録
	object3dCommon_ = object3dCommon;
	lightManager_ = lightManager;

	// リストの初期化
	obstacles_.clear();

	// JSONエディタに登録
	obstacleData_ = std::make_shared<ObstacleData>();
	JsonEditorManager::GetInstance()->Register("obstacles", obstacleData_);
}

void ObstacleManager::Update()
{
#ifdef _DEBUG
	ImGui::Begin("Obstacle Manager");

	ImGui::End();
#endif
	// 削除処理
	auto& transforms = obstacleData_->GetObstacles();
	while (transforms.size() < obstacles_.size())
	{
		// 障害物の数が減っている場合は削除
		obstacles_.pop_back();
	}

	// 障害物の数が変わったかチェック
	if (obstacleData_->GetObstacleCount() != obstacles_.size())
	{
		// 障害物の数が異なる場合は再生成
		CreateObstacles("wall.obj");
	}
	else
	{
		// 同じ数ならTransformだけ反映
		ApplyObstacleData();
	}
}

void ObstacleManager::Draw(CameraManager* camera)
{
	for (auto& obstacle : obstacles_)
	{
		if (obstacle)
		{
			if (culling_)
			{
				auto cameraPos = camera->GetActiveCamera()->GetTranslate();
				float distance = (obstacle->GetPosition() - cameraPos).Length();
				if (distance > 200.0f) // カメラからの距離が一定以上なら描画しない
				{
					continue;
				}
			}
			obstacle->Draw(camera); // 描画
		}
	}
}

void ObstacleManager::LoadObstacleData(const std::string& jsonName)
{
	obstacleData_->Initialize(jsonName);
}

void ObstacleManager::CreateObstacles(const std::string& modelName)
{
	// 既存の障害物をクリア
	obstacles_.clear();

	// 位置、回転、スケールの情報を取得
	auto& obstacleInfo = obstacleData_->GetObstacles();


	// 障害物を生成
	for (uint32_t i = 0; i < obstacleData_->GetObstacleCount(); ++i)
	{
		auto obstacle = std::make_unique<Obstacle>("Obstacle");
		obstacle->Initialize(object3dCommon_, lightManager_);
		obstacle->SetModel(modelName);
		obstacle->SetPosition(obstacleInfo[i].transform.translate);
		obstacle->SetRotation(obstacleInfo[i].transform.rotate);
		obstacle->SetScale(obstacleInfo[i].transform.scale);
		// 衝突判定コンポーネントを追加
		obstacle->AddComponent("OBBCollider", std::make_unique<OBBColliderComponent>(obstacle.get()));
		if (i == 0)
		{
			obstacle->GetModel()->SetUVScale(Vector3(10.0f, 10.0f, 1.0f));
		}
		obstacles_.push_back(std::move(obstacle));

	}
}

void ObstacleManager::ApplyObstacleData()
{
	// 障害物の位置、回転、スケールをデータから適用
	auto& obstacleInfo = obstacleData_->GetObstacles();
	for (size_t i = 0; i < obstacles_.size() && i < obstacleInfo.size(); ++i)
	{
		obstacles_[i]->SetPosition(obstacleInfo[i].transform.translate);
		obstacles_[i]->SetRotation(obstacleInfo[i].transform.rotate);
		obstacles_[i]->SetScale(obstacleInfo[i].transform.scale);
		obstacles_[i]->Update(); // 更新を呼び出して反映
	}
}

