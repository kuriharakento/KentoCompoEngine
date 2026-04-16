#include "AnnihilationSystem.h"
#include "engine/ecs/Registry.h"
#include "engine/ecs/system/ISystem.h"
#include "engine/ecs/components/TransformComponent.h"
#include "application/ecs/components/StatusComponent.h"
#include "application/ecs/components/ImpactChargeComponent.h"
#include "application/ecs/components/PlayerProgressionComponent.h"
#include "application/ecs/components/SkillComponent.h"
#include "engine/ecs/components/TagComponent.h"
#include "math/MathUtils.h"
#include "engine/effects/particle/ParticleManager.h"
#include "audio/Audio.h"

void AnnihilationSystem::Update(Registry& registry)
{
    // Registry::View は単一のコンポーネントのみをサポートするため、StatusComponent をメインビューとする
    auto view = registry.View<ecs::StatusComponent>();
    if (!view) return;

    for (uint32_t i = 0; i < view->GetSize(); ++i)
    {
        EntityID entity = view->GetEntityFromDenseIndex(i);
        auto& status = view->GetDataFromDenseIndex(i);

        // 死亡している場合
        if (!status.isAlive_)
        {
            // 必要なコンポーネント（ImpactChargeComponent）があるか確認して誘爆処理
            if (registry.HasComponent<ImpactChargeComponent>(entity))
            {
                auto& impact = registry.GetComponent<ImpactChargeComponent>(entity);
                if (impact.stackCount_ >= impact.kMaxStacks)
                {
                    TriggerExplosion(entity, registry);
                    // 誘爆は一度きり (フラグを折る)
                    impact.stackCount_ = 0;
                }
            }

            // プレイヤーにスコア・EXP加算
            auto progView = registry.View<PlayerProgressionComponent>();
            if (progView)
            {
                for (uint32_t j = 0; j < progView->GetSize(); ++j)
                {
                    progView->GetDataFromDenseIndex(j).totalScore_ += 100;
                    progView->GetDataFromDenseIndex(j).currentExp_ += 1.0f; // 1体 = 1EXP に変更
                }
            }

            // ビームチャージを加算
            auto skillView = registry.View<SkillComponent>();
            if (skillView)
            {
                for (uint32_t j = 0; j < skillView->GetSize(); ++j)
                {
                    auto& skill = skillView->GetDataFromDenseIndex(j);
                    skill.beamCharge_ += SkillComponent::kChargePerKill;
                    if (skill.beamCharge_ >= SkillComponent::kBeamChargeMax)
                    {
                        skill.beamCharge_ = SkillComponent::kBeamChargeMax;
                        skill.isBeamReady_ = true;
                    }
                }
            }

            // 最終的にエンティティを破棄
            if (registry.HasComponent<TransformComponent>(entity))
            {
                // 敵の場合のみ死亡パーティクルを再生
                bool isEnemy = false;
                if (registry.HasComponent<ecs::TagComponent>(entity))
                {
                    isEnemy = (registry.GetComponent<ecs::TagComponent>(entity).type == ecs::TagComponent::Type::Enemy);
                }

                if (isEnemy)
                {
                    auto& trans = registry.GetComponent<TransformComponent>(entity);
                    ParticleManager::GetInstance()->Play("enemy_death", trans.localPosition_);
                    Audio::GetInstance()->PlayWave("enemy_kill", false);
                }
            }

            registry.DestroyEntityDeferred(entity);
        }
    }
}

void AnnihilationSystem::TriggerExplosion(EntityID sourceEntity, Registry& registry)
{
    if (!registry.HasComponent<TransformComponent>(sourceEntity) || 
        !registry.HasComponent<ImpactChargeComponent>(sourceEntity)) return;

    auto& sourceTrans = registry.GetComponent<TransformComponent>(sourceEntity);
    auto& sourceImpact = registry.GetComponent<ImpactChargeComponent>(sourceEntity);
    
    // 周囲のエネミーを全走査 (TransformComponent をメインビューとする)
    auto view = registry.View<TransformComponent>();
    if (!view) return;

    for (uint32_t i = 0; i < view->GetSize(); ++i)
    {
        EntityID targetEntity = view->GetEntityFromDenseIndex(i);
        if (targetEntity == sourceEntity) continue;
        
        // 必要なコンポーネント（StatusComponent）があるか確認
        if (!registry.HasComponent<ecs::StatusComponent>(targetEntity)) continue;

        auto& tTrans = view->GetDataFromDenseIndex(i);
        auto& tStatus = registry.GetComponent<ecs::StatusComponent>(targetEntity);

        float distance = (tTrans.localPosition_ - sourceTrans.localPosition_).Length();
        if (distance <= sourceImpact.explosionRadius_)
        {
            // ダメージ適用
            float damage = sourceImpact.explosionDamage_Base_;
            tStatus.hp_.SetBase(tStatus.hp_.GetBase() - damage);
            
            // 誘爆スタック付与 (ImpactChargeComponent がある場合のみ)
            if (registry.HasComponent<ImpactChargeComponent>(targetEntity))
            {
                auto& tImpact = registry.GetComponent<ImpactChargeComponent>(targetEntity);
                tImpact.stackCount_ = (std::min)(static_cast<int>(tImpact.kMaxStacks), tImpact.stackCount_ + 1);
            }

            if (tStatus.hp_.GetBase() <= 0.0f)
            {
                tStatus.isAlive_ = false;
            }
        }
    }
}
