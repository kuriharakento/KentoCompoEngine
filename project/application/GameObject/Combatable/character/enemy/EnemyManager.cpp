#include "EnemyManager.h"

// ECS Refactored - Custom EnemyBase derivations removed
#include "application/combo/ComboManager.h"
#include "math/MathUtils.h"
#include <audio/Audio.h>
#include "effects/particle/ParticleManager.h"
#include "externals/imgui/imgui.h"
#include "time/TimeManager.h"

// ECS Components
#include "application/ecs/components/TransformComponent.h"
#include "application/ecs/components/EnemyStateComponent.h"
#include "application/ecs/components/RenderComponent.h"

// ECS Systems
#include "application/ecs/systems/EnemyBehaviorSystem.h"
#include "application/ecs/systems/InstancedRenderSystem.h"

// Engine
#include "graphics/3d/Object3dCommon.h"
#include "manager/graphics/ModelManager.h"

void EnemyManager::Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, CameraManager* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager, GameObject* target)
{
	object3dCommon_ = object3dCommon; // 3Dオブジェクト共通処理
	spriteCommon_ = spriteCommon; // スプライト共通処理
	camera_ = camera; // カメラマネージャー
	lightManager_ = lightManager; // ライトマネージャー
	shadowMapManager_ = shadowMapManager; // シャドウマップマネージャー
	target_ = target; // ターゲット（プレイヤーなど）
	
	// ECS Integration
	registry_ = std::make_unique<Registry>();
	registry_->Initialize(10000); // 最大10000体を想定（デバッグ用）
	registry_->RegisterComponent<TransformComponent>(10000);
	registry_->RegisterComponent<EnemyStateComponent>(10000);
	registry_->RegisterComponent<RenderComponent>(10000);

	// レンダラ群の初期化
	renderers_["enemy"] = std::make_unique<InstancedModelRenderer>(10000);
	
	// モデルを取得
	Model* enemyModel = ModelManager::GetInstance()->FindModel("enemy");
	if (!enemyModel) {
		ModelManager::GetInstance()->LoadModel("enemy");
		enemyModel = ModelManager::GetInstance()->FindModel("enemy");
	}

	renderers_["enemy"]->Initialize(
		object3dCommon_->GetDXCommon(),
		object3dCommon_->GetSrvManager(),
		enemyModel
	);
	// モデルやテクスチャの明示的割り当てが必要な場合はここで行うか上位レイヤーから流す
	
	// 敵キャラクターの出現範囲を設定
	emitRange_ = {
		{ -10.0f, 1.0f, -10.0f }, // 最小座標
		{ 10.0f, 1.0f, 10.0f }   // 最大座標
	};
}

void EnemyManager::Update()
{
	if (!registry_) return;

#ifdef USE_IMGUI
	ImGui::Begin("Enemy Manager");
	ImGui::Text("Active Entities: %d", registry_->GetActiveEntityCount());
	ImGui::Text("EnemyBase Instances: %zu", enemies_.size());
	
	if (ImGui::Button("Add Pistol Enemy (1)")) AddPistolEnemy(1);
	ImGui::SameLine();
	if (ImGui::Button("Add Pistol Enemy (100)")) AddPistolEnemy(100);

	if (ImGui::Button("Add Assault Enemy (1)")) AddAssaultEnemy(1);
	ImGui::SameLine();
	if (ImGui::Button("Add Assault Enemy (100)")) AddAssaultEnemy(100);

	if (ImGui::Button("Add Shotgun Enemy (1)")) AddShotgunEnemy(1);
	ImGui::SameLine();
	if (ImGui::Button("Add Shotgun Enemy (100)")) AddShotgunEnemy(100);

	if (ImGui::Button("Add Knife Enemy (1)")) AddKnifeEnemy(1);
	ImGui::SameLine();
	if (ImGui::Button("Add Knife Enemy (100)")) AddKnifeEnemy(100);

	ImGui::Separator();
	
	// ECSストレステスト用大量スポーン
	if (ImGui::Button("Stress Test: Spawn 1,000 Enemies")) AddAssaultEnemy(1000);
	
	// 全クリア
	if(ImGui::Button("Clear All Enemies"))
	{
		Clear();
	}
	ImGui::End();
#endif

	// ECS Systemパイプライン
	float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;
	EnemyBehaviorSystem::Update(*registry_, deltaTime);

	// 全滅判定（生成後に0になった瞬間だけ発火）
	// (互換性のため、enemies_ が空になったかどうかも見れるようにしておく)
	if (registry_->GetActiveEntityCount() == 0 && enemies_.empty() && onAllEnemiesDefeatedCallback_)
	{
		onAllEnemiesDefeatedCallback_();
		onAllEnemiesDefeatedCallback_ = nullptr; // 一度だけ呼ぶ
	}

	// 既存互換：実体オブジェクトの更新
	for (auto& enemy : enemies_)
	{
		enemy->Update();
	}

	// 寿命が尽きたEnemy実体の削除
	enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(),
		[](const std::unique_ptr<EnemyBase>& enemy) {
			return !enemy->IsAlive();
		}), enemies_.end());

	// フレーム末尾の遅延破棄実行
	registry_->FlushGarbageCollection();
}

void EnemyManager::UpdateTransform(CameraManager* camera)
{
	// 既存の実体に対するTransform更新
	for (auto& enemy : enemies_)
	{
		enemy->UpdateTransform(camera); // 各敵キャラクターのTransform情報を更新
	}
}

void EnemyManager::Draw3D(CameraManager* camera)
{
	// まず実体を従来のObject3d単位で描画
	for (auto& enemy : enemies_) { enemy->Draw3D(camera); }

	// ECS管理のInstancedRenderSystem描画
	if (registry_ && camera)
	{
		Camera* activeCamera = camera->GetActiveCamera();
		InstancedRenderSystem::DrawGrouped(*registry_, renderers_, activeCamera, lightManager_, shadowMapManager_);
	}
}

void EnemyManager::DrawShadow()
{
	for (auto& enemy : enemies_) { enemy->DrawShadow(); }
}

void EnemyManager::Draw2D()
{
	for (auto& enemy : enemies_) { enemy->Draw2D(); }
}

void EnemyManager::AddPistolEnemy(uint32_t count)
{
	if (!registry_) return;
	for (uint32_t i = 0; i < count; ++i)
	{
		EntityID entity = registry_->CreateEntity();
		if (entity == kInvalidEntity) continue;

		// ランダムな位置を設定
		Vector3 randomPosition = MathUtils::RandomVector3(emitRange_.min_, emitRange_.max_);
		
		TransformComponent transform;
		transform.localPosition = randomPosition;
		registry_->AddComponent<TransformComponent>(entity, transform);
		
		EnemyStateComponent state;
		// ピストル敵固有の初期化
		registry_->AddComponent<EnemyStateComponent>(entity, state);

		AssignRenderComponent(entity, "enemy");

		// 既存実体の生成
		auto enemy = std::make_unique<PistolEnemy>();
		Transform t;
		t.translate = randomPosition;
		enemy->Initialize(object3dCommon_, spriteCommon_, camera_, lightManager_, target_, t);
		enemies_.push_back(std::move(enemy));
	}
}

void EnemyManager::AddAssaultEnemy(uint32_t count)
{
	if (!registry_) return;
	for (uint32_t i = 0; i < count; ++i)
	{
		EntityID entity = registry_->CreateEntity();
		if (entity == kInvalidEntity) continue;

		Vector3 randomPosition = MathUtils::RandomVector3(emitRange_.min_, emitRange_.max_);
		
		TransformComponent transform;
		transform.localPosition = randomPosition;
		registry_->AddComponent<TransformComponent>(entity, transform);
		
		EnemyStateComponent state;
		registry_->AddComponent<EnemyStateComponent>(entity, state);

		AssignRenderComponent(entity, "enemy");

		// 既存実体の生成
		auto enemy = std::make_unique<AssaultEnemy>();
		Transform t;
		t.translate = randomPosition;
		enemy->Initialize(object3dCommon_, spriteCommon_, camera_, lightManager_, target_, t);
		enemies_.push_back(std::move(enemy));
	}
}

void EnemyManager::AddShotgunEnemy(uint32_t count)
{
	if (!registry_) return;
	for (uint32_t i = 0; i < count; ++i)
	{
		EntityID entity = registry_->CreateEntity();
		if (entity == kInvalidEntity) continue;

		Vector3 randomPosition = MathUtils::RandomVector3(emitRange_.min_, emitRange_.max_);
		
		TransformComponent transform;
		transform.localPosition = randomPosition;
		registry_->AddComponent<TransformComponent>(entity, transform);
		
		EnemyStateComponent state;
		registry_->AddComponent<EnemyStateComponent>(entity, state);

		AssignRenderComponent(entity, "enemy");

		// 既存実体の生成
		auto enemy = std::make_unique<ShotgunEnemy>();
		Transform t;
		t.translate = randomPosition;
		enemy->Initialize(object3dCommon_, spriteCommon_, camera_, lightManager_, target_, t);
		enemies_.push_back(std::move(enemy));
	}
}

void EnemyManager::AddKnifeEnemy(uint32_t count)
{
	if (!registry_) return;
	for (uint32_t i = 0; i < count; ++i)
	{
		EntityID entity = registry_->CreateEntity();
		if (entity == kInvalidEntity) continue;

		Vector3 randomPosition = MathUtils::RandomVector3(emitRange_.min_, emitRange_.max_);
		
		TransformComponent transform;
		transform.localPosition = randomPosition;
		registry_->AddComponent<TransformComponent>(entity, transform);
		
		EnemyStateComponent state;
		registry_->AddComponent<EnemyStateComponent>(entity, state);

		AssignRenderComponent(entity, "enemy");

		// 既存実体の生成
		auto enemy = std::make_unique<KnifeEnemy>();
		Transform t;
		t.translate = randomPosition;
		enemy->Initialize(object3dCommon_, spriteCommon_, camera_, lightManager_, target_, t);
		enemies_.push_back(std::move(enemy));
	}
}

void EnemyManager::SetEnemyData(const std::vector<GameObjectInfo>& data)
{
	enemyData_ = data;
	Clear();
	CreateAssaultEnemyFromData();
}

void EnemyManager::AddEnemiesFromGameObjectInfo(const std::vector<GameObjectInfo>& data)
{
	if (!registry_) return;
	for (int i = 0; i < data.size(); i++)
	{
		EntityID entity = registry_->CreateEntity();
		if (entity == kInvalidEntity) continue;

		TransformComponent transform;
		transform.localPosition = {data[i].transform.translate.x, data[i].transform.translate.y, data[i].transform.translate.z};
		// Note: rotate や scale も設定可能
		registry_->AddComponent<TransformComponent>(entity, transform);

		EnemyStateComponent state;
		registry_->AddComponent<EnemyStateComponent>(entity, state);

		AssignRenderComponent(entity, "enemy");

		// 既存実体の生成
		auto enemy = std::make_unique<AssaultEnemy>();
		Transform t;
		t.translate = data[i].transform.translate;
		enemy->Initialize(object3dCommon_, spriteCommon_, camera_, lightManager_, target_, t);
		enemies_.push_back(std::move(enemy));

		// スポーンパーティクルの生成
		ParticleManager::GetInstance()->Play("enemy_spawn_effect", data[i].transform.translate);
	}
}

void EnemyManager::Clear()
{
	enemies_.clear();

	// レジストリがない、またはクリア関数がない場合は初期化し直すか全破棄キューを入れるか
	// 今回の実装ではInitializeを再コールするか手動でFlushさせるなどの対応。
	if (registry_)
	{
		// TODO: 全Entityの破棄処理。簡易的には再度Initializeを呼ぶ
		registry_->Initialize(registry_->GetMaxEntityCount());
	}
}

void EnemyManager::CreateAssaultEnemyFromData()
{
	if (!registry_) return;
	for(int i = 0;i < enemyData_.size();i++)
	{
		EntityID entity = registry_->CreateEntity();
		if (entity == kInvalidEntity) continue;

		TransformComponent transform;
		transform.localPosition = {enemyData_[i].transform.translate.x, enemyData_[i].transform.translate.y, enemyData_[i].transform.translate.z};
		registry_->AddComponent<TransformComponent>(entity, transform);

		EnemyStateComponent state;
		registry_->AddComponent<EnemyStateComponent>(entity, state);

		AssignRenderComponent(entity, "enemy");

		// 既存実体の生成
		auto enemy = std::make_unique<AssaultEnemy>();
		Transform t;
		t.translate = enemyData_[i].transform.translate;
		enemy->Initialize(object3dCommon_, spriteCommon_, camera_, lightManager_, target_, t);
		enemies_.push_back(std::move(enemy));
	}
}

void EnemyManager::CleanupPendingRemovals()
{
	// ECS遅延評価に移行したため未使用
}

void EnemyManager::AssignRenderComponent(EntityID entity, const std::string& modelName)
{
	if (!registry_) return;
	
	RenderComponent render;
	render.modelName = modelName;
	render.useInstancing = true;
	render.isVisible = true;
	registry_->AddComponent<RenderComponent>(entity, render);
}
