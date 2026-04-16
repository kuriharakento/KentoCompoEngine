#pragma once
#include "engine/ecs/system/ISystem.h"

/**
 * @brief デコイのロジックや演出（エフェクト）を管理するシステム。
 */
class DecoySystem : public ISystem
{
public:
	void Update(Registry& registry) override;
};
