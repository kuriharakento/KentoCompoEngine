#pragma once

#include "ISystem.h"

namespace KCE
{
/**
 * @brief MovementComponent を持つエンティティの座標を更新するシステム。
 */
class MovementSystem : public ISystem
{
public:
    void Update(Registry& registry) override;
};
} // namespace KCE
