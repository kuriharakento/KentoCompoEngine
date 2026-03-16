#pragma once

#include "../../../engine/ecs/Registry.h"
#include "../components/LifetimeComponent.h"

/**
 * @brief 寿命が尽きたEntityを破棄するシステム
 */
class LifetimeSystem
{
public:
    /**
     * @brief 寿命の更新と破棄予約
     * @param registry 対象のRegistry
     * @param deltaTime フレーム間の経過時間
     */
    static void Update(Registry& registry, float deltaTime);
};
