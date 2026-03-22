#pragma once
#include "engine/ecs/system/ISystem.h"

class Registry;

/**
 * @brief StatusComponentのライフサイクル（StatusValueの更新など）を管理するシステム。
 */
class EcsStatusSystem : public ISystem
{
public:
    EcsStatusSystem() = default;
    ~EcsStatusSystem() override = default;

    void Update(Registry& registry) override;
};
