#include "ProgressionSystem.h"
#include "engine/ecs/Registry.h"
#include "application/ecs/components/PlayerProgressionComponent.h"
#include "application/ecs/components/SkillComponent.h"
#include "application/ecs/components/StatusComponent.h"

void ProgressionSystem::Update(Registry& registry)
{
    // Registry::View は単一のコンポーネントのみをサポートするため、PlayerProgressionComponent をメインビューとする
    auto view = registry.View<PlayerProgressionComponent>();
    if (!view) return;

    for (uint32_t i = 0; i < view->GetSize(); ++i)
    {
        EntityID entity = view->GetEntityFromDenseIndex(i);
        auto& prog = view->GetDataFromDenseIndex(i);
        
        // --- 経験値チェック ---
        while (prog.currentExp_ >= prog.nextLevelExp_)
        {
            prog.currentExp_ -= prog.nextLevelExp_;
            prog.level_++;
            
            // 次のレベルの目標値を更新 (インクリメンタルな増加)
            prog.nextLevelExp_ *= 1.2f;

            // 報酬の適用
            ApplyLevelUpRewards(entity, prog.level_, registry);
        }
    }
}

void ProgressionSystem::ApplyLevelUpRewards(EntityID entity, uint32_t newLevel, Registry& registry)
{
    if (registry.HasComponent<SkillComponent>(entity))
    {
        auto& skill = registry.GetComponent<SkillComponent>(entity);
        
        // スキル解放テーブル (Rev.4)
        if (newLevel == 2) skill.isRmbUnlocked_ = true;
        if (newLevel == 3) skill.isDecoyUnlocked_ = true;
        if (newLevel == 4) skill.isImpactUnlocked_ = true;
        if (newLevel == 5) skill.isBeamUnlocked_ = true;
    }

    if (registry.HasComponent<ecs::StatusComponent>(entity))
    {
        auto& status = registry.GetComponent<ecs::StatusComponent>(entity);
        
        // ステータス強化
        float boost = 1.1f; // 10%増
        status.maxHp_.SetBase(status.maxHp_.GetBase() * boost);
        status.hp_.SetBase(status.hp_.GetBase() * boost); // 回復を兼ねるか検討
        status.attackPower_.SetBase(status.attackPower_.GetBase() * boost);
        status.moveSpeed_.SetBase(status.moveSpeed_.GetBase() * 1.05f); // 速度は控えめに
    }
}
