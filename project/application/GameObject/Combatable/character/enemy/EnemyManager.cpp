#include "EnemyManager.h"

#include "AssaultEnemy.h"
#include "KnifeEnemy.h"
#include "PistolEnemy.h"
#include "ShotgunEnemy.h"
#include "application/combo/ComboManager.h"
#include "math/MathUtils.h"

void EnemyManager::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target)
{
	object3dCommon_ = object3dCommon;
	lightManager_ = lightManager;
	target_ = target;
	enemies_.clear();
	
	// 敵の出現範囲を設定
	emitRange_ = {
		{ -10.0f, 1.0f, -10.0f },
		{ 10.0f, 1.0f, 10.0f }
	};

	// 死亡エフェクトの初期化
	deathEffect_ = std::make_unique<EnemyDeathEffect>();
	deathEffect_->Initialize();
}

void EnemyManager::Update()
{
#ifdef USE_IMGUI
	ImGui::Begin("Enemy Manager");
	ImGui::Text("Enemy Count: %d", static_cast<int>(enemies_.size()));
	if (ImGui::Button("Add Pistol Enemy"))
	{
		AddPistolEnemy(1);
	}
	if (ImGui::Button("Add Assault Enemy"))
	{
		AddAssaultEnemy(1);
	}
	if (ImGui::Button("Add Shotgun Enemy"))
	{
		AddShotgunEnemy(1);
	}
	if (ImGui::Button("Add Knife Enemy"))
	{
		AddKnifeEnemy(1);
	}

	if(ImGui::Button("Clear All Enemies"))
	{
		enemies_.clear();
	}

	ImGui::SeparatorText("Enemies Info");

	for (size_t i = 0; i < enemies_.size(); ++i)
	{
		auto& enemy = enemies_[i];
		ImGui::Separator();
		ImGui::Text("Enemy #%zu", i);
		Vector3 pos = enemy->GetPosition();
		ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
		ImGui::Text("Type: %s", typeid(*enemy).name());
	}
	ImGui::End();

#endif

	for (auto& enemy : enemies_)
	{
		enemy->Update();
	}

	// 死亡した敵の処理と遅延破棄
	for (auto it = enemies_.begin(); it != enemies_.end();)
	{
		if (!(*it)->IsAlive())
		{
			// 死亡エフェクトとコンボ加算
			deathEffect_->PlayDeathEffect((*it)->GetPosition(), EnemyDeathEffect::EffectType::Electric);
			ComboManager::GetInstance().OnEnemyDefeated();
			
			// カメラシェイク演出
			if(camera_)
			{
				camera_->GetActiveCamera()->StartShake(0.45f, 0.3f);
			}

			// 即座に破棄せず遅延破棄リストに移動（描画中の不正アクセス防止）
			pendingRemovals_.push_back(std::move(*it));
			it = enemies_.erase(it);
		}
		else
		{
			++it;
		}
	}

	// 全滅判定（ウェーブシステムへの通知）
	if (enemies_.empty() && onAllEnemiesDefeatedCallback_)
	{
		onAllEnemiesDefeatedCallback_();
		onAllEnemiesDefeatedCallback_ = nullptr;  // 一度だけ実行
	}
}

void EnemyManager::UpdateTransform(CameraManager* camera)
{
	for (auto& enemy : enemies_)
	{
		enemy->UpdateTransform(camera);
	}
}

void EnemyManager::Draw(CameraManager* camera)
{
	for (auto& enemy : enemies_)
	{
		enemy->Draw(camera);
	}

	// Draw終了後にデストラクタを呼び出して安全に破棄
	CleanupPendingRemovals();
}

void EnemyManager::AddPistolEnemy(uint32_t count)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		auto enemy = std::make_unique<PistolEnemy>();
		enemy->Initialize(object3dCommon_, lightManager_, target_);
		Vector3 randomPosition = MathUtils::RandomVector3(emitRange_.min_, emitRange_.max_);
		enemy->SetPosition(randomPosition);
		enemies_.push_back(std::move(enemy));
	}
}

void EnemyManager::AddAssaultEnemy(uint32_t count)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		auto enemy = std::make_unique<AssaultEnemy>();
		enemy->Initialize(object3dCommon_, lightManager_, target_);
		Vector3 randomPosition = MathUtils::RandomVector3(emitRange_.min_, emitRange_.max_);
		enemy->SetPosition(randomPosition);
		enemies_.push_back(std::move(enemy));
	}
}

void EnemyManager::AddShotgunEnemy(uint32_t count)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		auto enemy = std::make_unique<ShotgunEnemy>();
		enemy->Initialize(object3dCommon_, lightManager_, target_);
		Vector3 randomPosition = MathUtils::RandomVector3(emitRange_.min_, emitRange_.max_);
		enemy->SetPosition(randomPosition);
		enemies_.push_back(std::move(enemy));
	}
}

void EnemyManager::AddKnifeEnemy(uint32_t count)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		auto enemy = std::make_unique<KnifeEnemy>();
		enemy->Initialize(object3dCommon_, lightManager_, target_);
		Vector3 randomPosition = MathUtils::RandomVector3(emitRange_.min_, emitRange_.max_);
		enemy->SetPosition(randomPosition);
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
		// 敵キャラクターの種類に応じて生成
		if (data[i].fileName == "enemy" || data[i].fileName == "assault")
		{
			auto enemy = std::make_unique<AssaultEnemy>();
			enemy->Initialize(object3dCommon_, lightManager_, target_, Transform(data[i].transform.scale, data[i].transform.rotate, data[i].transform.translate));
			enemy->SetModel(data[i].fileName);
			enemies_.push_back(std::move(enemy));
		}
		else if (data[i].fileName == "knife")
		{
			auto enemy = std::make_unique<KnifeEnemy>();
			enemy->Initialize(object3dCommon_, lightManager_, target_, Transform(data[i].transform.scale, data[i].transform.rotate, data[i].transform.translate));
			enemy->SetModel("cube");
			enemies_.push_back(std::move(enemy));
		}
	}
}

void EnemyManager::Clear()
{
	enemies_.clear();
}

void EnemyManager::CreateAssaultEnemyFromData()
{
	for(int i = 0;i < enemyData_.size();i++)
	{
		auto enemy = std::make_unique<AssaultEnemy>();
		enemy->Initialize(object3dCommon_, lightManager_, target_);
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
		pendingRemovals_.clear();
	}
}
