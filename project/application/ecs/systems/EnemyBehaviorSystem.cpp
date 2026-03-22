#include "EnemyBehaviorSystem.h"
#include "engine/time/TimeManager.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/EnemyAIComponent.h"
#include "application/ecs/components/StatusComponent.h"
#include "engine/gameobject/component/collision/RayColliderComponent.h"
#include "application/gameObject/base/GameObjectTag.h" // For Player tag if present

void EnemyBehaviorSystem::Update(Registry& registry)
{
    if (!registry.HasComponentArray<EnemyAIComponent>()) return;

    float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;
    auto aiView = registry.View<EnemyAIComponent>();
    if (!aiView) return;

    // プレイヤーエンティティを検索してターゲットとする
    // （本来は各EnemyにTarget情報を設定するが、とりあえず最初のPlayerタグを探す）
    EntityID playerEntity = kInvalidEntity;
    // Note: If you have a TagComponent, you could query it here. 
    // Assuming simple global logic for demo purposes, we'll try to find a target below.
    // (Actually, the target is usually set in StageManager or similar)

    for (uint32_t i = 0; i < aiView->GetSize(); ++i)
    {
        EntityID entity = aiView->GetEntityFromDenseIndex(i);
        if (!registry.HasComponent<TransformComponent>(entity)) continue;
        if (!registry.HasComponent<ecs::StatusComponent>(entity)) continue;

        EnemyAIComponent& ai = aiView->GetDataFromDenseIndex(i);
        ecs::StatusComponent& status = registry.GetComponent<ecs::StatusComponent>(entity);
        TransformComponent& transform = registry.GetComponent<TransformComponent>(entity);

        // 各種タイマーを更新
        ai.stateTimer_ += deltaTime;
        ai.strafeTimer_ += deltaTime;
        ai.positionCheckTimer_ += deltaTime;
        ai.combatStateTimer_ += deltaTime;
        if (ai.spawnTimer_ < 2.0f) // kSpawnDuration
        {
            ai.spawnTimer_ += deltaTime;
        }

        if (ai.actionCooldown_ > 0) ai.actionCooldown_ -= deltaTime;

        // 前回位置の保存 (スタック検知用)
        ai.lastPosition_ = transform.localPosition_;

        // ターゲットエンティティの存在チェックと位置取得
        Vector3 targetPos;
        bool hasTarget = false;
        if (ai.targetEntity_ != kInvalidEntity && registry.IsAlive(ai.targetEntity_))
        {
            if (registry.HasComponent<TransformComponent>(ai.targetEntity_)) {
                targetPos = registry.GetComponent<TransformComponent>(ai.targetEntity_).localPosition_;
                hasTarget = true;
            }
        }


        // BTがなければスキップ
        if (!ai.behaviorTree_) continue;

        // Blackboardへ状態情報をセット
        auto& bb = ai.behaviorTree_->GetBlackboard();
        bb.Set<EntityID>("Owner", entity);
        bb.Set<Registry*>("Registry", &registry);
        bb.Set<EntityID>("Target", ai.targetEntity_);
        bb.Set<Vector3>("TargetPosition", targetPos);
        
        bool isInAttackRange = IsInAttackRange(registry, entity, ai);
        bool isInExtendedAttackRange = IsInExtendedAttackRange(registry, entity, ai);

        bb.Set<bool>("IsTargetVisible", true);
        bb.Set<bool>("IsInAttackRange", isInAttackRange);
        bb.Set<bool>("IsInExtendedAttackRange", isInExtendedAttackRange);
        bb.Set<float>("StateTimer", ai.stateTimer_);
        bb.Set<float>("StrafeTimer", ai.strafeTimer_);
        bb.Set<float>("MoveSpeed", status.moveSpeed_.GetValue());
        bb.Set<float>("AttackRange", ai.attackRange_);
        bb.Set<float>("MinRange", ai.minRange_);
        bb.Set<float>("MaxRange", ai.maxRange_);
        bb.Set<float>("ExtendedMinRange", ai.extendedMinRange_);
        bb.Set<float>("ExtendedMaxRange", ai.extendedMaxRange_);
        bb.Set<float>("DetectionRange", ai.detectionRange_);
        bb.Set<float>("SpawnTimer", ai.spawnTimer_);
        bb.Set<float>("SpawnDuration", 2.0f); // kSpawnDuration
        bb.Set<int>("CurrentPatrolIndex", ai.currentPatrolIndex_);
        bb.Set<bool>("PatrolInitialized", ai.patrolInitialized_);

        // ビヘイビアツリーを実行
        ai.behaviorTree_->Tick();
    }
}

bool EnemyBehaviorSystem::IsTargetVisible(Registry& registry, EntityID entity, EnemyAIComponent& ai)
{
    if (ai.targetEntity_ == kInvalidEntity) return false;
    if (!registry.HasComponent<TransformComponent>(ai.targetEntity_)) return false;
    
    auto& ownerTransform = registry.GetComponent<TransformComponent>(entity);
    auto& targetTransform = registry.GetComponent<TransformComponent>(ai.targetEntity_);

    Vector3 direction = targetTransform.localPosition_ - ownerTransform.localPosition_;
    float distance = direction.Length();

    return (distance <= ai.detectionRange_);
}

bool EnemyBehaviorSystem::IsInAttackRange(Registry& registry, EntityID entity, EnemyAIComponent& ai)
{
    if (ai.targetEntity_ == kInvalidEntity) return false;
    if (!registry.HasComponent<TransformComponent>(ai.targetEntity_)) return false;

    auto& ownerTransform = registry.GetComponent<TransformComponent>(entity);
    auto& targetTransform = registry.GetComponent<TransformComponent>(ai.targetEntity_);

    float distance = (targetTransform.localPosition_ - ownerTransform.localPosition_).Length();
    return (distance >= ai.minRange_ && distance <= ai.maxRange_);
}

bool EnemyBehaviorSystem::IsInExtendedAttackRange(Registry& registry, EntityID entity, EnemyAIComponent& ai)
{
    if (ai.targetEntity_ == kInvalidEntity) return false;
    if (!registry.HasComponent<TransformComponent>(ai.targetEntity_)) return false;

    auto& ownerTransform = registry.GetComponent<TransformComponent>(entity);
    auto& targetTransform = registry.GetComponent<TransformComponent>(ai.targetEntity_);

    float distance = (targetTransform.localPosition_ - ownerTransform.localPosition_).Length();
    return (distance >= ai.extendedMinRange_ && distance <= ai.extendedMaxRange_);
}
