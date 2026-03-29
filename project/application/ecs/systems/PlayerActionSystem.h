#pragma once
#include "engine/ecs/system/ISystem.h"
#include "engine/ecs/Entity.h"

class CameraManager;
class SystemManager;

/**
 * @brief プレイヤーの入力に基づき、移動やスキルの発動を制御するシステム。
 * 
 * - 各スキル（LMB, RMB, Q, E, R）の解放判定とクールタイム管理を行う。
 * - 移動・回避ロジックもここに集約。
 */
class PlayerActionSystem : public ISystem
{
public:
    void Update(Registry& registry) override;

    void SetCameraManager(CameraManager* cameraManager) { cameraManager_ = cameraManager; }
    void SetSystemManager(SystemManager* systemManager) { systemManager_ = systemManager; }

private:
    // 回避（Dodge）の更新
    void UpdateDodge(EntityID entity, Registry& registry, float deltaTime);
    // 通常移動の更新
    void UpdateMovement(EntityID entity, Registry& registry, float deltaTime);
    // スキルの更新
    void UpdateSkills(EntityID entity, Registry& registry, float deltaTime);

    // 各スキルの個別処理
    void UpdateLMB(EntityID entity, Registry& registry, float deltaTime);
    void UpdateRMB(EntityID entity, Registry& registry, float deltaTime);
    void UpdateQ(EntityID entity, Registry& registry, float deltaTime);
    void UpdateE(EntityID entity, Registry& registry, float deltaTime);
    void UpdateR(EntityID entity, Registry& registry, float deltaTime);

    CameraManager* cameraManager_ = nullptr;
    SystemManager* systemManager_ = nullptr;
};
