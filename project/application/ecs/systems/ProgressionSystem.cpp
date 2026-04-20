#include "ProgressionSystem.h"
#include "engine/ecs/Registry.h"
#include "application/ecs/components/PlayerProgressionComponent.h"
#include "application/ecs/components/SkillComponent.h"
#include "application/ecs/components/StatusComponent.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/effects/particle/ParticleManager.h"
#include "engine/manager/effect/PostProcessManager.h"
#include "engine/time/TimeManager.h"
#include "engine/math/Easing.h"
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

            // スキル選択が必要なら一旦中断（次フレームで選択後に残りEXPを処理する）
            if (pendingSkillSelection_) break;
        }

        if (leveledUp)
        {
            PlayLevelUpEffects(entity, registry);
        }
    }

    // --- ビネット演出の更新 ---
    if (isVignetteActive_ && postProcessManager_ && postProcessManager_->vignetteEffect_)
    {
        float dt = TimeManager::GetInstance().GetGameContext().deltaTime;
        vignetteTimer_ += dt;

        float progress = std::clamp(vignetteTimer_ / kVignetteDuration, 0.0f, 1.0f);
        
        // パルス強度: 最初は強く、徐々に消える (EaseOutQuad で減衰)
        float intensity = 0.8f * (1.0f - EaseOutQuad(progress));

        auto& vignette = postProcessManager_->vignetteEffect_;
        vignette->SetEnabled(true);
        vignette->SetColor({ 1.0f, 1.0f, 0.0f }); // 黄色
        vignette->SetIntensity(intensity);

        if (progress >= 1.0f)
        {
            isVignetteActive_ = false;
            vignette->SetEnabled(false);
            vignette->SetColor({ 0.0f, 0.0f, 0.0f }); // 元に戻す
        }
    }
}

void ProgressionSystem::ApplyLevelUpRewards(EntityID entity, uint32_t newLevel, Registry& registry)
{
    if (registry.HasComponent<SkillComponent>(entity))
    {
        auto& skill = registry.GetComponent<SkillComponent>(entity);

        if (newLevel == 2 || newLevel == 3)
        {
            // LV2/LV3: スキル選択UIで処理する（ここでは何もしない）
            // SkillSelectionUI が ProgressionSystem から呼ばれる
            pendingSkillSelection_ = true;
            pendingSelectionLevel_ = newLevel;
        }
        else if (newLevel >= 4)
        {
            // LV4以降: Rスキル解放
            skill.isBeamUnlocked_ = true;

            // LV4以降: 3枚の候補からランダムに、アップグレードを選択
            pendingSkillSelection_ = true;
            pendingSelectionLevel_ = newLevel;
        }
    }
}

void ProgressionSystem::PlayLevelUpEffects(EntityID entity, Registry& registry)
{
    // 1. VFX (エフェクト名を仮定。後で追加可能)
    if (registry.HasComponent<TransformComponent>(entity))
    {
        auto& trans = registry.GetComponent<TransformComponent>(entity);
        ParticleManager::GetInstance()->Play("level_up", trans.localPosition_);
    }

    // 2. SFX (サウンドを仮定)
    // Audio::Play("level_up_se");

    // 3. PostProcess (黄色いビネットのパルス演出)
    if (postProcessManager_ && postProcessManager_->vignetteEffect_)
    {
        isVignetteActive_ = true;
        vignetteTimer_ = 0.0f;
        
        // 初期状態を設定
        postProcessManager_->vignetteEffect_->SetEnabled(true);
        postProcessManager_->vignetteEffect_->SetColor({ 1.0f, 1.0f, 0.0f });
        postProcessManager_->vignetteEffect_->SetIntensity(0.8f);
    }

    // 4. UI通知
    if (levelUpUI_)
    {
        levelUpUI_->Trigger();
    }
}
