#pragma once
#include "engine/ecs/system/ISystem.h"
#include "application/ecs/components/TurretComponent.h"

class SystemManager;

/**
 * @brief タレットの自動攻撃を管理するシステム。
 *
 * - TurretComponent を持つエンティティを毎フレーム処理
 * - 最寄りの敵に向けて自動射撃を行う
 */
class TurretSystem : public ISystem
{
public:
	void Update(Registry& registry) override;

	void SetSystemManager(SystemManager* systemManager) { systemManager_ = systemManager; }

private:
	void UpdateLaserBeam(EntityID turretEntity, TurretComponent& turret, Registry& registry, float dt);
	SystemManager* systemManager_ = nullptr;
};
