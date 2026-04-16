#pragma once
#include "engine/ecs/system/ISystem.h"
#include "engine/ecs/Entity.h"

class CameraManager;
class SystemManager;

/**
 * @brief プレイヤーの入力に基づき、移動やスキルの発動を制御するシステム。
 *
 * - LMB: 通常攻撃（初期装備）
 * - Q: スキル1（ボム派生: ホーミングミサイル or スプリンクラー）
 * - E: スキル2（タレット: ダメージアップ or 連鎖）
 * - R: ビーム（チャージ制）
 * - 移動・回避ロジックもここに集約。
 */
class PlayerActionSystem : public ISystem
{
public:
	void Update(Registry& registry) override;

	void SetCameraManager(CameraManager* cameraManager) { cameraManager_ = cameraManager; }
	void SetSystemManager(SystemManager* systemManager) { systemManager_ = systemManager; }
	void SetObject3dCommon(class Object3dCommon* common) { object3dCommon_ = common; }

private:
	// 回避（Dodge）の更新
	void UpdateDodge(EntityID entity, Registry& registry, float deltaTime);
	// 通常移動の更新
	void UpdateMovement(EntityID entity, Registry& registry, float deltaTime);
	// スキルの更新
	void UpdateSkills(EntityID entity, Registry& registry, float deltaTime);

	// 各スキルの個別処理
	void UpdateLMB(EntityID entity, Registry& registry, float deltaTime);
	void UpdateSkill1(EntityID entity, Registry& registry, float deltaTime);
	void UpdateSkill2(EntityID entity, Registry& registry, float deltaTime);
	void UpdateR(EntityID entity, Registry& registry, float deltaTime);

	// スキル1の派生処理
	void FireBombWave(EntityID entity, Registry& registry);
	void FireHomingMissile(EntityID entity, Registry& registry);

	// スキル2の派生処理
	void SpawnDecoy(EntityID entity, Registry& registry);
	void SpawnTurret(EntityID entity, Registry& registry);

	// 誘爆を発生させる
	void SpawnExplosion(EntityID sourceEntity, Registry& registry);

	CameraManager* cameraManager_ = nullptr;
	SystemManager* systemManager_ = nullptr;
	class Object3dCommon* object3dCommon_ = nullptr;
};
