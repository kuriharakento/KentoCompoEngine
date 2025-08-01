#include "EnemyManager.h"

#include "application/GameObject/character/enemy/AssaultEnemy.h"
#include "application/GameObject/character/enemy/PistolEnemy.h"
#include "application/GameObject/character/enemy/ShotgunEnemy.h"
#include "ImGui/imgui_internal.h"
#include "math/MathUtils.h"

void EnemyManager::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target)
{
	object3dCommon_ = object3dCommon; // 3Dオブジェクト共通処理
	lightManager_ = lightManager; // ライトマネージャー
	target_ = target; // ターゲット（プレイヤーなど）
	enemies_.clear(); // 敵キャラクターのリストをクリア
	// 敵キャラクターの出現範囲を設定
	emitRange_ = {
		{ -10.0f, 1.0f, -10.0f }, // 最小座標
		{ 10.0f, 1.0f, 10.0f }   // 最大座標
	};

	// エフェクトの初期化
	deathEffect_ = std::make_unique<EnemyDeathEffect>();
	deathEffect_->Initialize();
}

void EnemyManager::Update()
{
#ifdef _DEBUG
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
	ImGui::End();

#endif

	for (auto& enemy : enemies_)
	{
		enemy->Update(); // 各敵キャラクターの更新
	}

	// 敵キャラクターのリストから死亡した敵を削除
	for (auto it = enemies_.begin(); it != enemies_.end();)
	{
		if (!(*it)->IsAlive())
		{
			deathEffect_->PlayDeathEffect((*it)->GetPosition(),EnemyDeathEffect::EffectType::Electric); // 死亡エフェクトを再生
			it = enemies_.erase(it); // 死亡した敵を削除
		}
		else
		{
			++it; // 次の敵へ
		}
	}
}

void EnemyManager::UpdateTransform(CameraManager* camera)
{
	for (auto& enemy : enemies_)
	{
		enemy->UpdateTransform(camera); // 各敵キャラクターのTransform情報を更新
	}
}

void EnemyManager::Draw(CameraManager* camera)
{
	for (auto& enemy : enemies_)
	{
		enemy->Draw(camera); // 各敵キャラクターの描画
	}
}

void EnemyManager::AddPistolEnemy(uint32_t count)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		auto enemy = std::make_unique<PistolEnemy>();
		enemy->Initialize(object3dCommon_, lightManager_, target_);
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
		enemy->Initialize(object3dCommon_, lightManager_, target_);
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
		enemy->Initialize(object3dCommon_, lightManager_, target_);
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
