#pragma once
#include "application/stage/StageData.h"
#include "base/EnemyBase.h"
#include "application/gameObject/combatable/character/enemy/PistolEnemy.h"
#include "application/gameObject/combatable/character/enemy/AssaultEnemy.h"
#include "application/gameObject/combatable/character/enemy/ShotgunEnemy.h"
#include "application/gameObject/combatable/character/enemy/KnifeEnemy.h"
#include "math/AABB.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include "engine/ecs/Registry.h"
#include "engine/ecs/Entity.h"
#include "engine/graphics/3d/InstancedModelRenderer.h"

class ShadowMapManager;

class EnemyManager
{
public:
	void Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, CameraManager* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager, GameObject* target);
	void Update();
	void UpdateTransform(CameraManager* camera);
	void Draw3D(CameraManager* camera);
	void DrawShadow();
	void Draw2D();
	
	// ECS Integration: Registryへのアクセスを提供（所有権は EnemyManager が持つ）
	Registry* GetRegistry() const { return registry_.get(); }
	// 全滅判定（EnemyBaseのリストベースか、あるいはECSのカウントか）
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

	// Entity生成時にRenderComponentを付与する共通処理
	void AssignRenderComponent(EntityID entity, const std::string& modelName);

	// 敵全滅時のコールバック関数
	std::function<void()> onAllEnemiesDefeatedCallback_ = nullptr;

private:
	Object3dCommon* object3dCommon_ = nullptr; // 3Dオブジェクト共通処理
	SpriteCommon* spriteCommon_ = nullptr; // スプライト共通処理
	LightManager* lightManager_ = nullptr; // ライトマネージャー
	GameObject* target_ = nullptr; // ターゲット（プレイヤーなど）
	CameraManager* camera_ = nullptr; // カメラマネージャー
	ShadowMapManager* shadowMapManager_ = nullptr; // シャドウマップマネージャー
	AABB emitRange_ = {};
	
	// ECS Integration (独占所有状態)
	std::unique_ptr<Registry> registry_;
	
	// Renderers for ECS entities (モデル名 -> Renderer)
	std::unordered_map<std::string, std::unique_ptr<InstancedModelRenderer>> renderers_;
	
	// 実体としての敵オブジェクト管理（既存描画互換パス）
	std::vector<std::unique_ptr<EnemyBase>> enemies_;

	// 敵データ
	std::vector<GameObjectInfo> enemyData_;
};
