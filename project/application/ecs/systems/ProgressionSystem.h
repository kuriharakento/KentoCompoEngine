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
class LevelUpUI;
class PostProcessManager;

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

    // 演出・UIの連携用
    void SetLevelUpUI(LevelUpUI* ui) { levelUpUI_ = ui; }
    void SetPostProcessManager(PostProcessManager* ppm) { postProcessManager_ = ppm; }

private:
    /**
     * @brief レベルアップの報酬（ステータス強化・スキル解放）を適用。
     */
    void ApplyLevelUpRewards(EntityID entity, uint32_t newLevel, Registry& registry);

    /**
     * @brief レベルアップ時の演出（VFX, SFX, PostProcess）を再生。
     */
    void PlayLevelUpEffects(EntityID entity, Registry& registry);

private:
    LevelUpUI* levelUpUI_ = nullptr;
    PostProcessManager* postProcessManager_ = nullptr;
};
