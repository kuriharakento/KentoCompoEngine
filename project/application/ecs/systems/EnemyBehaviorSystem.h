#pragma once

#include "engine/ecs/Registry.h"
#include "engine/ecs/system/ISystem.h"
#include "application/ecs/components/EnemyTypeComponent.h"
#include "math/Vector3.h"

/**
 * @brief 敵のAIと振る舞いを更新するシステム (Pure ECS版)
 * EnemyAIComponent 内の BehaviorTree をTickして評価します。
 */
class EnemyBehaviorSystem : public ISystem
{
public:
    /**
     * @brief 振る舞いの更新
     * @param registry 対象のRegistry
     */
    void Update(Registry& registry) override;

private:
    // 型別の更新処理
    void UpdateMeleeBehavior(EntityID entity, Registry& registry, const Vector3& playerPos, float dt);
    
    // 遠距離型（将来用）
    void UpdateRangedBehavior(EntityID entity, Registry& registry, const Vector3& playerPos, float dt);

    // ヘルパー関数
    bool IsInAttackRange(Registry& registry, EntityID entity, struct EnemyAIComponent& ai);
};
