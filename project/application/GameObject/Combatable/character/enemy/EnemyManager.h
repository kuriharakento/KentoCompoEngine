#pragma once
#include "application/effect/EnemyDeathEffect.h"
#include "application/stage/StageData.h"
#include "base/EnemyBase.h"
#include "math/AABB.h"

class LightManager;
class Object3dCommon;

class EnemyManager
{
public:
	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target);
	void Update();
	void UpdateTransform(CameraManager* camera);
	void Draw(CameraManager* camera);
	const std::vector<std::unique_ptr<EnemyBase>>& GetEnemies() const { return enemies_; }
	uint32_t GetEnemyCount() const { return static_cast<uint32_t>(enemies_.size()); }
	void AddPistolEnemy(uint32_t count);
	void AddAssaultEnemy(uint32_t count);
	void AddShotgunEnemy(uint32_t count);
	void SetEnemyData(const std::vector<GameObjectInfo>& data);
	void SetTarget(GameObject* target) { target_ = target; }
	void Clear();

private:
	void CreateAssaultEnemyFromData();

private:
	Object3dCommon* object3dCommon_ = nullptr; // 3Dオブジェクト共通処理
	LightManager* lightManager_ = nullptr; // ライトマネージャー
	GameObject* target_ = nullptr; // ターゲット（プレイヤーなど）
	AABB emitRange_ = {};
	// 敵リスト
	std::vector<std::unique_ptr<EnemyBase>> enemies_;
	// 敵データ
	std::vector<GameObjectInfo> enemyData_;
	// 死亡パーティクル
	std::unique_ptr<EnemyDeathEffect> deathEffect_;
};

