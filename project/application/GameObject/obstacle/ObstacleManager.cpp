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
	obstacleData_.clear();
}

void ObstacleManager::Update()
{
#ifdef _DEBUG
	ImGui::Begin("Obstacle Manager");

	ImGui::Text("Obstacle Count: %zu", obstacles_.size());
	for (size_t i = 0; i < obstacles_.size(); ++i)
	{
		auto& obstacle = obstacles_[i];
		if (obstacle)
		{
			ImGui::Separator();
			ImGui::Text("Index: %zu", i);
			ImGui::Text("Tag: %s", obstacle->GetTag().c_str());

			const Vector3& pos = obstacle->GetPosition();
			ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);

			const Vector3& rot = obstacle->GetRotation();
			ImGui::Text("Rotation: (%.2f, %.2f, %.2f)", rot.x, rot.y, rot.z);

			const Vector3& scale = obstacle->GetScale();
			ImGui::Text("Scale: (%.2f, %.2f, %.2f)", scale.x, scale.y, scale.z);
		}
	}

	ImGui::End();
#endif

	for (auto& obstacle : obstacles_)
	{
		if (obstacle)
		{
			obstacle->Update(); // 各障害物の更新
		}
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

void ObstacleManager::Clear()
{
	// 障害物のリストをクリア
	obstacles_.clear();
}

void ObstacleManager::CreateObstacles(const std::string& modelName)
{
	// 既存の障害物をクリア
	obstacles_.clear();

	// 障害物を生成
	for (uint32_t i = 0; i < obstacleData_.size(); ++i)
	{
		auto obstacle = std::make_unique<Obstacle>("Obstacle");
		obstacle->Initialize(object3dCommon_, lightManager_);
		obstacle->SetModel(modelName);
		obstacle->SetPosition(obstacleData_[i].transform.translate);
		obstacle->SetRotation(obstacleData_[i].transform.rotate);
		obstacle->SetScale(obstacleData_[i].transform.scale);
		if (i == 0)
		{
			obstacle->GetModel()->SetUVScale(Vector3(10.0f, 10.0f, 1.0f));
		}
		obstacles_.push_back(std::move(obstacle));

	}
}

void ObstacleManager::ApplyObstacleData()
{
	for (int i = 0; i < obstacles_.size(); i++)
	{
		if (i < obstacleData_.size())
		{
			auto& obstacle = obstacles_[i];
			if (obstacle)
			{
				obstacle->SetPosition(obstacleData_[i].transform.translate);
				obstacle->SetRotation(obstacleData_[i].transform.rotate);
				obstacle->SetScale(obstacleData_[i].transform.scale);
				obstacle->Update();
			}
		}
		else
		{
			break; // データがない場合はループを抜ける
		}
	}
}

void ObstacleManager::SetObstacleData(const std::vector<GameObjectInfo>& data)
{
	obstacleData_ = data;

	// 障害物の生成
	CreateObstacles("wall");
}

