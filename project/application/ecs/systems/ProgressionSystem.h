#pragma once
#include "engine/ecs/system/ISystem.h"
#include "engine/ecs/Entity.h"
#include <cstdint>

/**
 * @brief 経験値の獲得とレベルアップを管理するシステム。
 * 
 * - レベルアップ時に一度だけ SkillComponent のフラグを更新し、
 *   StatusComponent の数値を強化する。
 */
class ProgressionSystem : public ISystem
{
public:
    void Update(Registry& registry) override;

private:
    /**
     * @brief レベルアップの報酬（ステータス強化・スキル解放）を適用。
     */
    void ApplyLevelUpRewards(EntityID entity, uint32_t newLevel, Registry& registry);
};
