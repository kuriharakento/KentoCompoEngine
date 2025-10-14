#include "ObstacleManager.h"

#include <unordered_set>

#include "BarrierBlock.h"
#include "application/GameObject/component/collision/OBBColliderComponent.h"
#include "manager/editor/JsonEditorManager.h"

void ObstacleManager::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager)
{
	// ポインタをメンバ変数に記録
	object3dCommon_ = object3dCommon;
	lightManager_ = lightManager;

	// リストの初期化
	obstacles_.clear();
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

	// 新しい障害物データの同期
	SyncNewObstacleData();

	// 障害物の更新
	ApplyObstacleData();
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

void ObstacleManager::CreateObstacles()
{
	// 既存の障害物をクリア
	obstacles_.clear();

	// 障害物を生成
	auto obstacles = obstacleData_->GetObstacles();
	for (const auto& obstacle : obstacles)
	{
		if (obstacle.type == "BarrierBlock")
		{
			CreateBarrierBlock(obstacle);
		}
		else // デフォルトは通常の障害物
		{
			CreateObstacle(obstacle);
		}
	}
}

void ObstacleManager::ApplyObstacleData()
{
	if (!obstacleData_) return;
	auto data = obstacleData_->GetObstacles();
	for (auto& obstacle : obstacles_)
	{
		for (auto& info : data)
		{
			if (obstacle->GetName() == info.name)
			{
				// Jsonの配置情報を適用
				obstacle->SetPosition(info.transform.translate);
				obstacle->SetRotation(info.transform.rotate);
				obstacle->SetScale(info.transform.scale);
				// コンポーネントの更新
				obstacle->Update();
				break;
			}
		}
	}
}

void ObstacleManager::LoadObstacleData(const std::string& path)
{
	// 障害物データの読み込み
	obstacleData_->Initialize(path);
	// 生成
	CreateObstacles();
}

void ObstacleManager::SetObstacleData(ObstacleData* data)
{
	// 障害物データの設定
	obstacleData_ = data;
	// 障害物の生成
	CreateObstacles();
}

void ObstacleManager::CreateObstacle(const GameObjectInfo& info)
{
	auto obstacle = std::make_unique<Obstacle>();
	obstacle->Initialize(object3dCommon_, lightManager_);
	obstacle->SetModel("wall");
	obstacle->SetPosition(info.transform.translate);
	obstacle->SetRotation(info.transform.rotate);
	obstacle->SetScale(info.transform.scale);
	obstacle->SetName(info.name);
	obstacles_.push_back(std::move(obstacle));
}

void ObstacleManager::CreateBarrierBlock(const GameObjectInfo& info)
{
	auto obstacle = std::make_unique<BarrierBlock>();
	obstacle->Initialize(object3dCommon_, lightManager_);
	obstacle->SetModel("wall");
	obstacle->SetPosition(info.transform.translate);
	obstacle->SetRotation(info.transform.rotate);
	obstacle->SetScale(info.transform.scale);
	obstacle->SetName(info.name);
	obstacles_.push_back(std::move(obstacle));
}

void ObstacleManager::SyncNewObstacleData()
{
	if (!obstacleData_) return;
	auto data = obstacleData_->GetObstacles();

	// 既存障害物の名前リストを作成
	std::unordered_set<std::string> existingNames;
	for (const auto& obj : obstacles_)
	{
		if (obj) existingNames.insert(obj->GetName());
	}

	// データ側でまだ存在しないものだけ生成
	for (const auto& info : data)
	{
		if (existingNames.count(info.name) == 0)
		{
			if (info.type == "BarrierBlock")
				CreateBarrierBlock(info);
			else
				CreateObstacle(info);
		}
	}
}

