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
class SkillSelectionUI;

/**
 * @brief 経験値の獲得とレベルアップを管理するシステム。
 * 
 * - LV2/LV3ではスキル選択UIを表示してゲームを一時停止。
 * - LV4以降は通常攻撃の自動強化。
 */
class ProgressionSystem : public ISystem
{
public:
    void Update(Registry& registry) override;

    // 演出・UIの連携用
    void SetLevelUpUI(LevelUpUI* ui) { levelUpUI_ = ui; }
    void SetPostProcessManager(PostProcessManager* ppm) { postProcessManager_ = ppm; }
    void SetSkillSelectionUI(SkillSelectionUI* ui) { skillSelectionUI_ = ui; }

    // スキル選択待ちかどうか
    bool IsPendingSkillSelection() const { return pendingSkillSelection_; }
    uint32_t GetPendingSelectionLevel() const { return pendingSelectionLevel_; }
    void ClearPendingSkillSelection() { pendingSkillSelection_ = false; pendingSelectionLevel_ = 0; }

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
    SkillSelectionUI* skillSelectionUI_ = nullptr;

    // --- スキル選択待ち ---
    bool pendingSkillSelection_ = false;
    uint32_t pendingSelectionLevel_ = 0;

    // --- ビネット演出用 ---
    bool isVignetteActive_ = false;  // ビネット演出中か
    float vignetteTimer_ = 0.0f;     // 経過時間
    const float kVignetteDuration = 1.2f; // 演出の長さ（秒）
};
