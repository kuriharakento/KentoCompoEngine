#pragma once
#include "application/stage/StageData.h"
#include "base/EnemyBase.h"
#include "math/AABB.h"
#include <vector>
#include <memory>

class LightManager;
class Object3dCommon;

class EnemyManager
{
public:
	void Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, CameraManager* camera, LightManager* lightManager, GameObject* target);
	void Update();
	void UpdateTransform(CameraManager* camera);
	void Draw3D(CameraManager* camera);
	void DrawShadow();
	void Draw2D();
	const std::vector<std::unique_ptr<EnemyBase>>& GetEnemies() const { return enemies_; }
	uint32_t GetEnemyCount() const { return static_cast<uint32_t>(enemies_.size()); }
	void AddPistolEnemy(uint32_t count);
	void AddAssaultEnemy(uint32_t count);
	void AddShotgunEnemy(uint32_t count);
	void AddKnifeEnemy(uint32_t count);
	void SetEnemyData(const std::vector<GameObjectInfo>& data);
	void AddEnemiesFromGameObjectInfo(const std::vector<GameObjectInfo>& data);
	void SetTarget(GameObject* target) { target_ = target; }
	void SetCameraManager(CameraManager* camera) { camera_ = camera; }
	void SetOnAllEnemiesDefeatedCallback(std::function<void()> callback) { onAllEnemiesDefeatedCallback_ = std::move(callback); }
	void Clear();

private:
	void CreateAssaultEnemyFromData();

	// フレーム末クリーンアップ（外部から呼ぶ必要はありません。Draw 内で呼び出します）
	void CleanupPendingRemovals();

	// 敵全滅時のコールバック関数
	std::function<void()> onAllEnemiesDefeatedCallback_ = nullptr;

private:
	Object3dCommon* object3dCommon_ = nullptr; // 3Dオブジェクト共通処理
	SpriteCommon* spriteCommon_ = nullptr; // スプライト共通処理
	LightManager* lightManager_ = nullptr; // ライトマネージャー
	GameObject* target_ = nullptr; // ターゲット（プレイヤーなど）
	CameraManager* camera_ = nullptr; // カメラマネージャー
	AABB emitRange_ = {};
	// 敵リスト
	std::vector<std::unique_ptr<EnemyBase>> enemies_;
	// フレーム末に破棄するために一時的に保持するコンテナ
	std::vector<std::unique_ptr<EnemyBase>> pendingRemovals_;
	// 敵データ
	std::vector<GameObjectInfo> enemyData_;
};

