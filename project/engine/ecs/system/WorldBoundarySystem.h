#pragma once
#include "engine/ecs/system/ISystem.h"

/**
 * @brief エンティティの座標を円形のポピュレーション範囲内に制限するシステム。
 */
class WorldBoundarySystem : public ISystem
{
public:
    /**
     * @brief 座標の制限を適用する。
     * @param registry ECSレジストリ
     */
    void Update(Registry& registry) override;
};
