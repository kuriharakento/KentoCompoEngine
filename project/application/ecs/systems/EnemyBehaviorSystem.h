#pragma once

#include "../../../engine/ecs/Registry.h"
#include "../components/TransformComponent.h"
#include "../components/EnemyStateComponent.h"

/**
 * @brief 敵のAIと振る舞いを更新するシステム
 */
class EnemyBehaviorSystem
{
public:
    /**
     * @brief 振る舞いの更新
     * @param registry 対象のRegistry
     * @param deltaTime フレーム間の経過時間
     */
    static void Update(Registry& registry, float deltaTime);
};
