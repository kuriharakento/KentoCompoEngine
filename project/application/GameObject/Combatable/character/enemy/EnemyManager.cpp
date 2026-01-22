#include "EnemyManager.h"

#include "AssaultEnemy.h"
#include "KnifeEnemy.h"
#include "PistolEnemy.h"
#include "ShotgunEnemy.h"
#include "application/combo/ComboManager.h"
#include "math/MathUtils.h"

void EnemyManager::Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, CameraManager* camera, LightManager* lightManager, GameObject* target)
{
	object3dCommon_ = object3dCommon; // 3Dオブジェクト共通処理
	spriteCommon_ = spriteCommon; // スプライト共通処理
	camera_ = camera; // カメラマネージャー
	lightManager_ = lightManager; // ライトマネージャー
	target_ = target; // ターゲット（プレイヤーなど）
	enemies_.clear(); // 敵キャラクターのリストをクリア
	// 敵キャラクターの出現範囲を設定
	emitRange_ = {
		{ -10.0f, 1.0f, -10.0f }, // 最小座標
		{ 10.0f, 1.0f, 10.0f }   // 最大座標
	};
}

void EnemyManager::Update()
{
	// 削除保留中の敵をクリーンアップ
	CleanupPendingRemovals();

#ifdef USE_IMGUI
	ImGui::Begin("Enemy Manager");
	ImGui::Text("Enemy Count: %d", static_cast<int>(enemies_.size()));
	if (ImGui::Button("Add Pistol Enemy"))
	{
		AddPistolEnemy(1); // ピストル敵を1体追加
	}
	if (ImGui::Button("Add Assault Enemy"))
	{
		AddAssaultEnemy(1); // アサルト敵を1体追加
	}
	if (ImGui::Button("Add Shotgun Enemy"))
	{
		AddShotgunEnemy(1); // ショットガン敵を1体追加
	}
	if (ImGui::Button("Add Knife Enemy"))
	{
		AddKnifeEnemy(1); // ナイフ敵を1体追加
	}

	// 敵の削除
	if(ImGui::Button("Clear All Enemies"))
	{
		enemies_.clear();
	}

	ImGui::SeparatorText("Enemies Info");

	// 各敵の情報表示
	for (size_t i = 0; i < enemies_.size(); ++i)
	{
		auto& enemy = enemies_[i];
		ImGui::Separator();
		ImGui::Text("Enemy #%zu", i);
		Vector3 pos = enemy->GetPosition();
		ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
		ImGui::Text("Type: %s", typeid(*enemy).name()); // 型名表示
	}
	ImGui::End();

#endif

	for (auto& enemy : enemies_)
	{
		enemy->Update(); // 各敵キャラクターの更新
	}

	// 死亡した敵を enemies_ から取り除き、実際の破棄は pendingRemovals_ に移動して遅延させる
	for (auto it = enemies_.begin(); it != enemies_.end();)
	{
		if (!(*it)->IsAlive())
		{
			// 死亡時処理（コンボ）
			// TODO: JSONベースのパーティクルエフェクトに置き換える
			ComboManager::GetInstance().OnEnemyDefeated();
			if(camera_)
			{
				camera_->GetActiveCamera()->StartShake(0.45f, 0.3f); // カメラを揺らす
			}

			// 移動して破棄を遅延
			pendingRemovals_.push_back(std::move(*it));
			it = enemies_.erase(it);
		}
		else
		{
			++it;
		}
	}

	// 全滅判定（enemies_ が空になった時点で発火させる）
	if (enemies_.empty() && onAllEnemiesDefeatedCallback_)
	{
		onAllEnemiesDefeatedCallback_();
		onAllEnemiesDefeatedCallback_ = nullptr; // 一度だけ呼ぶ
	}
}

void EnemyManager::UpdateTransform(CameraManager* camera)
{
	for (auto& enemy : enemies_)
	{
		enemy->UpdateTransform(camera); // 各敵キャラクターのTransform情報を更新
	}
}

void EnemyManager::Draw3D(CameraManager* camera)
{
	for (auto& enemy : enemies_)
	{
		enemy->Draw3D(camera); // 各敵キャラクターの描画
	}
}

void EnemyManager::DrawShadow()
{
	for (auto& enemy : enemies_)
	{
		enemy->DrawShadow(); // 各敵キャラクターのシャドウ描画
	}
}

void EnemyManager::Draw2D()
{
	for (auto& enemy : enemies_)
	{
		enemy->Draw2D(); // 各敵キャラクターの2D描画
	}	
}

void EnemyManager::AddPistolEnemy(uint32_t count)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		auto enemy = std::make_unique<PistolEnemy>();
		enemy->Initialize(
			object3dCommon_,
			spriteCommon_,
			camera_,
			lightManager_, 
			target_
		);
		//ランダムな位置を設定
		Vector3 randomPosition = MathUtils::RandomVector3(emitRange_.min_, emitRange_.max_);
		enemy->SetPosition(randomPosition);
		// 敵キャラクターを追加
		enemies_.push_back(std::move(enemy));
	}
}

void EnemyManager::AddAssaultEnemy(uint32_t count)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		auto enemy = std::make_unique<AssaultEnemy>();
		enemy->Initialize(
			object3dCommon_,
			spriteCommon_,
			camera_,
			lightManager_,
			target_
		);
		//ランダムな位置を設定
		Vector3 randomPosition = MathUtils::RandomVector3(emitRange_.min_, emitRange_.max_);
		enemy->SetPosition(randomPosition);
		// 敵キャラクターを追加
		enemies_.push_back(std::move(enemy));
	}
}

void EnemyManager::AddShotgunEnemy(uint32_t count)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		auto enemy = std::make_unique<ShotgunEnemy>();
		enemy->Initialize(
			object3dCommon_,
			spriteCommon_,
			camera_,
			lightManager_,
			target_
		);
		//ランダムな位置を設定
		Vector3 randomPosition = MathUtils::RandomVector3(emitRange_.min_, emitRange_.max_);
		enemy->SetPosition(randomPosition);
		// 敵キャラクターを追加
		enemies_.push_back(std::move(enemy));
	}
}

void EnemyManager::AddKnifeEnemy(uint32_t count)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		auto enemy = std::make_unique<KnifeEnemy>();
		enemy->Initialize(
			object3dCommon_,
			spriteCommon_,
			camera_,
			lightManager_,
			target_
		);
		//ランダムな位置を設定
		Vector3 randomPosition = MathUtils::RandomVector3(emitRange_.min_, emitRange_.max_);
		enemy->SetPosition(randomPosition);
		// 敵キャラクターを追加
		enemies_.push_back(std::move(enemy));
	}
}

void EnemyManager::SetEnemyData(const std::vector<GameObjectInfo>& data)
{
	enemyData_ = data;
	enemies_.clear();
	CreateAssaultEnemyFromData();
}

void EnemyManager::AddEnemiesFromGameObjectInfo(const std::vector<GameObjectInfo>& data)
{
	for (int i = 0; i < data.size(); i++)
	{
		// NOTE:今は無理やりやっているがファクトリーパターンなどで拡張性を持たせるべき
		// NOTE:処理がかぶっているのはKnifeのモデル用意していないからそれのせいです
		// 敵キャラクターの種類に応じて生成
		// アサルトの生成
		if (data[i].fileName == "enemy" || data[i].fileName == "assault")
		{
			auto enemy = std::make_unique<AssaultEnemy>();
			enemy->Initialize(
				object3dCommon_, 
				spriteCommon_,
				camera_,
				lightManager_, 
				target_,
				Transform(data[i].transform.scale, data[i].transform.rotate, data[i].transform.translate)
			);
			enemy->SetModel(data[i].fileName);
			
			enemies_.push_back(std::move(enemy));
		}
		// ナイフの生成
		else if (data[i].fileName == "knife")
		{
			auto enemy = std::make_unique<KnifeEnemy>();
			enemy->Initialize(
				object3dCommon_,
				spriteCommon_,
				camera_,
				lightManager_, 
				target_,
				Transform(data[i].transform.scale, data[i].transform.rotate, data[i].transform.translate)
			);
			// NOTE:ここのせいで処理が増えている。本来はもっと簡潔になります。
			enemy->SetModel("cube");
			enemies_.push_back(std::move(enemy));
		}
	}
}

void EnemyManager::Clear()
{
	enemies_.clear(); // 敵キャラクターのリストをクリア
}

void EnemyManager::CreateAssaultEnemyFromData()
{
	for(int i = 0;i < enemyData_.size();i++)
	{
		auto enemy = std::make_unique<AssaultEnemy>();
		enemy->Initialize(
			object3dCommon_,
			spriteCommon_,
			camera_,
			lightManager_,
			target_
		);
		enemy->SetModel(enemyData_[i].fileName);
		enemy->SetPosition(enemyData_[i].transform.translate);
		enemy->SetRotation(enemyData_[i].transform.rotate);
		enemy->SetScale(enemyData_[i].transform.scale);
		enemies_.push_back(std::move(enemy));
	}
}

void EnemyManager::CleanupPendingRemovals()
{
	if (!pendingRemovals_.empty())
	{
		// ここで unique_ptr をスコープ外にして破棄される
		pendingRemovals_.clear();
	}
}
