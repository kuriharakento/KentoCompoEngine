#pragma once


#include "ISystem.h"
#include "../../../engine/ecs/Registry.h"
#include "../components/LifetimeComponent.h"

/**
 * @brief 寿命が尽きたEntityを破?E??るシスチE??
 */
class LifetimeSystem : public ISystem
{
public:
    /**
     * @brief 寿命の更新と破?E???E
     * @param registry 対象のRegistry
     * @param deltaTime フレーム間?E経過時間
     */
    void Update(Registry& registry) override;
};
