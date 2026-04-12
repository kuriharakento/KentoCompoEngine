#include "ProgressionSystem.h"
#include "engine/ecs/Registry.h"
#include "application/ecs/components/PlayerProgressionComponent.h"
#include "application/ecs/components/SkillComponent.h"
#include "application/ecs/components/StatusComponent.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/effects/particle/ParticleManager.h"
#include "engine/manager/effect/PostProcessManager.h"
#include "application/UI/LevelUpUI.h"

void ProgressionSystem::Update(Registry& registry)
{
    // Registry::View は単一のコンポーネントのみをサポートするため、PlayerProgressionComponent をメインビューとする
    auto view = registry.View<PlayerProgressionComponent>();
    if (!view) return;

    for (uint32_t i = 0; i < view->GetSize(); ++i)
    {
        EntityID entity = view->GetEntityFromDenseIndex(i);
        auto& prog = view->GetDataFromDenseIndex(i);
        
        bool leveledUp = false;
        // --- 経験値チェック ---
        while (prog.currentExp_ >= prog.nextLevelExp_)
        {
            prog.currentExp_ -= prog.nextLevelExp_;
            prog.level_++;
            
            // 次のレベルの目標値を更新 (ユーザー指定のテーブル準拠)
            if (prog.level_ == 2) prog.nextLevelExp_ = 15.0f;
            else if (prog.level_ == 3) prog.nextLevelExp_ = 25.0f;
            else if (prog.level_ == 4) prog.nextLevelExp_ = 50.0f;
            else if (prog.level_ == 5) prog.nextLevelExp_ = 80.0f;
            else prog.nextLevelExp_ *= 1.5f; // Lv.6以降は1.5倍ずつ増加

            // 報酬の適用
            ApplyLevelUpRewards(entity, prog.level_, registry);
            leveledUp = true;
        }

        if (leveledUp)
        {
            PlayLevelUpEffects(entity, registry);
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

void ProgressionSystem::PlayLevelUpEffects(EntityID entity, Registry& registry)
{
    // 1. VFX (エフェクト名を仮定。後で追加可能)
    if (registry.HasComponent<TransformComponent>(entity))
    {
        auto& trans = registry.GetComponent<TransformComponent>(entity);
        // ParticleManager::GetInstance()->Play("level_up", trans.localPosition_);
    }

    // 2. SFX (サウンドを仮定)
    // Audio::Play("level_up_se");

    // 3. PostProcess (ビネットのパルス演出)
    if (postProcessManager_ && postProcessManager_->vignetteEffect_)
    {
        // ビネットを一瞬強める (本来はタイマー管理が必要だが、
        // 今回の要請では「器」を作ることに専念する)
        // postProcessManager_->vignetteEffect_->SetEnabled(true);
        // postProcessManager_->vignetteEffect_->SetIntensity(0.8f);
    }

    // 4. UI通知
    if (levelUpUI_)
    {
        levelUpUI_->Trigger();
    }
}
