#pragma once

#include "engine/ecs/Registry.h"
#include "engine/ecs/system/ISystem.h"

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
    // ヘルパー関数
    bool IsTargetVisible(Registry& registry, EntityID entity, struct EnemyAIComponent& ai);
    bool IsInAttackRange(Registry& registry, EntityID entity, struct EnemyAIComponent& ai);
    bool IsInExtendedAttackRange(Registry& registry, EntityID entity, struct EnemyAIComponent& ai);
};
