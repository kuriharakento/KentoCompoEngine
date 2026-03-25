#include "ObstacleManager.h"

#include <unordered_set>

#include "BarrierBlock.h"
#include "engine/gameobject/component/collision/OBBColliderComponent.h"
#include "manager/editor/JsonEditorManager.h"
#include "externals/imgui/imgui.h"

// ECS Components
#include "engine/ecs/components/TagComponent.h"
#include "engine/ecs/components/ColliderComponent.h"
#include "engine/ecs/components/CollisionLayerComponent.h"
#include "engine/ecs/components/CollisionResponseComponent.h"
#include "engine/math/AABB.h"


void ObstacleManager::Initialize(Registry* registry, Object3dCommon* object3dCommon, LightManager* lightManager)
{
	registry_ = registry;
	object3dCommon_ = object3dCommon;
	lightManager_ = lightManager;
	obstacles_.clear();
}

void ObstacleManager::Update()
{
#ifdef USE_IMGUI
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

void ObstacleManager::UpdateTransforms(CameraManager* camera)
{
	for (auto& obstacle : obstacles_)
	{
		if (obstacle)
		{
			obstacle->UpdateTransform(camera);
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

void ObstacleManager::DrawShadow()
{
	for (auto& obstacle : obstacles_)
	{
		if (obstacle)
		{
			if (culling_)
			{
				// シャドウマップ描画でもカリングを行うか？要検討だが、
				// パフォーマンス向上のため同様の距離チェックを入れておく
				// (カメラからの距離が遠すぎる影は描画しない)
				// ※ 本来はライト方向からの視点でカリングすべきだが、簡易的にカメラ距離を使用
				/*
				// 厳密にはシャドウカリングはライト視点が適切だが、
				// 簡易実装として通常のDrawと同じ距離制限をかける場合：
				auto cameraPos = object3dCommon_->GetDefaultCamera()->GetTranslate();
				float distance = (obstacle->GetPosition() - cameraPos).Length();
				if (distance > 200.0f)
				{
					continue;
				}
				*/
			}
			obstacle->DrawShadow(); // シャドウ描画
		}
	}
}

void ObstacleManager::Clear()
{
	obstacles_.clear();

	// ECS側も全エンティティを破棄・再初期化
	if (registry_)
	{
		registry_->Initialize(registry_->GetMaxEntityCount());
	}
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
		else if (obstacle.type == "Floor")
		{
			CreateFloor(obstacle);
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

	// ECS側にトランスフォーム属性を登録
	RegisterToRegistry(info, ObstacleComponent::Type::Obstacle, true);
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

	// ECS側に登録
	RegisterToRegistry(info, ObstacleComponent::Type::BarrierBlock, true);
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
			else if (info.type == "Floor")
				CreateFloor(info);
			else
				CreateObstacle(info);
		}
	}
}

void ObstacleManager::CreateFloor(const GameObjectInfo& info)
{
	// コライダーなしの床オブジェクト
	auto floor = std::make_unique<Obstacle>(gameObjectTag::item::Floor);
	floor->GameObject::Initialize(object3dCommon_, lightManager_);
	floor->SetModel(info.fileName);
	floor->SetPosition(info.transform.translate);
	floor->SetRotation(info.transform.rotate);
	floor->SetScale(info.transform.scale);
	floor->SetName(info.name);
	obstacles_.push_back(std::move(floor));
	// ECS側に登録（床はコライダーなし）
	RegisterToRegistry(info, ObstacleComponent::Type::Floor, false);
}

void ObstacleManager::RegisterToRegistry(const GameObjectInfo& info, ObstacleComponent::Type type, bool hasCollider)
{
	if (!registry_) return;

	EntityID entity = registry_->CreateEntity();
	if (entity == kInvalidEntity) return;

	// Tag
	registry_->AddComponent<TagComponent>(entity, { TagComponent::Type::Obstacle });

	// Transform
	TransformComponent transform;
	transform.localPosition_ = {
		info.transform.translate.x,
		info.transform.translate.y,
		info.transform.translate.z
	};
	transform.localRotation_ = info.transform.rotate;
	transform.localScale_ = info.transform.scale;
	registry_->AddComponent<TransformComponent>(entity, transform);

	// 描画設定
	InstancedRenderComponent render;
	render.modelName_ = info.fileName.empty() ? "wall" : info.fileName;
	render.useInstancing_ = true; // 障害物もインスタンシング対象にする（性能向上のため）
	registry_->AddComponent<InstancedRenderComponent>(entity, render);

	// 障害物属性
	ObstacleComponent obstacle;
	obstacle.type = type;
	obstacle.hasCollider = hasCollider;
	registry_->AddComponent<ObstacleComponent>(entity, obstacle);

	// 当たり判定 (OBB)
	if (hasCollider)
	{
		ColliderComponent col;
		col.type_ = ColliderType::OBB;
		// [Fix] 初期位置を同期（原点への押し戻しを防ぐ）
		col.previousPosition_ = transform.localPosition_;
		// obb_.size は CollisionSystem で worldScale を直接使うため設定不要
		registry_->AddComponent<ColliderComponent>(entity, col);

		CollisionLayerComponent layer;
		layer.category_ = CollisionLayerComponent::kObstacle;
		layer.mask_ = CollisionLayerComponent::kPlayer | CollisionLayerComponent::kEnemy | CollisionLayerComponent::kPlayerBullet | CollisionLayerComponent::kEnemyBullet;
		registry_->AddComponent<CollisionLayerComponent>(entity, layer);

		registry_->AddComponent<CollisionResponseComponent>(entity, {});
	}
}
