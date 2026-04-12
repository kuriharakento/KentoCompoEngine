#include "EnemyBehaviorSystem.h"
#include "engine/time/TimeManager.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/EnemyAIComponent.h"
#include "engine/ecs/components/TagComponent.h"
#include "application/ecs/components/StatusComponent.h"
#include "math/MathUtils.h"
#include "application/ecs/components/InducedExplosionComponent.h"
#include "application/ecs/components/EnemyTypeComponent.h"
#include "application/ecs/components/EnemyChargerComponent.h"
#include "engine/manager/graphics/LineManager.h"
#include "engine/math/VectorColorCodes.h"
#include <algorithm>
#include <cmath>

void EnemyBehaviorSystem::Update(Registry& registry)
{
    float dt = TimeManager::GetInstance().GetGameContext().deltaTime;

    // プレイヤーの座標取得
    EntityID playerEntity = kInvalidEntity;
    std::vector<Vector3> decoyPositions;
    auto tagView = registry.View<ecs::TagComponent>();
    if (tagView)
    {
        for (uint32_t i = 0; i < tagView->GetSize(); ++i)
        {
            EntityID ent = tagView->GetEntityFromDenseIndex(i);
            auto type = tagView->GetDataFromDenseIndex(i).type;
            if (type == ecs::TagComponent::Type::Player)
            {
                playerEntity = ent;
            }
            else if (type == ecs::TagComponent::Type::Decoy)
            {
                if (registry.HasComponent<TransformComponent>(ent))
                {
                    decoyPositions.push_back(registry.GetComponent<TransformComponent>(ent).localPosition_);
                }
            }
        }
    }

    if (playerEntity == kInvalidEntity || !registry.HasComponent<TransformComponent>(playerEntity)) return;
    const Vector3& playerPos = registry.GetComponent<TransformComponent>(playerEntity).localPosition_;

    // 各敵を更新
    auto aiView = registry.View<EnemyAIComponent>();
    if (!aiView) return;

    for (uint32_t i = 0; i < aiView->GetSize(); ++i)
    {
        EntityID entity = aiView->GetEntityFromDenseIndex(i);
        if (!registry.HasComponent<EnemyTypeComponent>(entity) || !registry.HasComponent<TransformComponent>(entity)) continue;

        const Vector3& enemyPos = registry.GetComponent<TransformComponent>(entity).localPosition_;

        // ターゲット選定（プレイヤーまたは最も近いデコイ）
        Vector3 targetPos = playerPos;
        float minTargetDistSq = (playerPos - enemyPos).LengthSquared();

        for (const auto& dPos : decoyPositions)
        {
            float distSq = (dPos - enemyPos).LengthSquared();
            if (distSq < minTargetDistSq)
            {
                minTargetDistSq = distSq;
                targetPos = dPos;
            }
        }

        auto& typeComp = registry.GetComponent<EnemyTypeComponent>(entity);
        switch (typeComp.type)
        {
        case EnemyType::Melee:
            UpdateMeleeBehavior(entity, registry, targetPos, dt);
            break;
        case EnemyType::Charger:
            UpdateChargerBehavior(entity, registry, targetPos, dt);
            break;
        }

        // 誘爆スタックの可視化
        if (registry.HasComponent<ecs::InducedExplosionComponent>(entity))
        {
            auto& explosion = registry.GetComponent<ecs::InducedExplosionComponent>(entity);
            auto& trans = registry.GetComponent<TransformComponent>(entity);

            // スタック数に応じた色
            Vector4 color = VectorColorCodes::Lime;
            if (explosion.count_ == 2) color = VectorColorCodes::Orange;
            else if (explosion.count_ >= 3) color = VectorColorCodes::Red;

            // 敵の大きさに合わせて表示位置を調整（頭の上に十分な余白を開ける）
            float markerHeight = trans.localScale_.y + 2.0f;
            Vector3 markerPos = { trans.localPosition_.x, markerHeight, trans.localPosition_.z };
            LineManager::GetInstance()->DrawCube(markerPos, 0.4f, color);
        }

        // 死亡判定
        if (registry.HasComponent<ecs::StatusComponent>(entity))
        {
            auto& status = registry.GetComponent<ecs::StatusComponent>(entity);
            if (status.hp_.GetValue() <= 0.0f)
            {
                status.isAlive_ = false;
            }
        }
    }
}

void EnemyBehaviorSystem::UpdateMeleeBehavior(EntityID entity, Registry& registry, const Vector3& playerPos, float dt)
{
    if (!registry.HasComponent<TransformComponent>(entity) || 
        !registry.HasComponent<ecs::StatusComponent>(entity)) return;

    auto& trans = registry.GetComponent<TransformComponent>(entity);
    auto& status = registry.GetComponent<ecs::StatusComponent>(entity);

    Vector3 toPlayer = playerPos - trans.localPosition_;
    float distance = toPlayer.Length();
    
    // 接近してくる (1.5f まで)
    if (distance > 1.5f)
    {
        toPlayer.NormalizeSelf();
        float speed = status.moveSpeed_.GetValue();
        float moveAmount = (std::min)(speed * dt, distance - 1.5f);
        trans.localPosition_ = trans.localPosition_ + toPlayer * moveAmount;
        
        // 回転 (プレイヤー方向)
        float targetYaw = std::atan2(toPlayer.x, toPlayer.z);
        trans.localRotation_.y = MathUtils::LerpAngle(trans.localRotation_.y, targetYaw, 0.1f);
        trans.isDirty_ = true;
    }
}

void EnemyBehaviorSystem::UpdateRangedBehavior(EntityID, Registry&, const Vector3&, float)
{
    // 将来用：射撃型などのロジックをここに書く
}

void EnemyBehaviorSystem::UpdateChargerBehavior(EntityID entity, Registry& registry, const Vector3& playerPos, float dt)
{
    if (!registry.HasComponent<TransformComponent>(entity) || 
        !registry.HasComponent<ecs::StatusComponent>(entity) ||
        !registry.HasComponent<EnemyChargerComponent>(entity)) return;

    auto& trans = registry.GetComponent<TransformComponent>(entity);
    auto& status = registry.GetComponent<ecs::StatusComponent>(entity);
    auto& charger = registry.GetComponent<EnemyChargerComponent>(entity);

    Vector3 toPlayer = playerPos - trans.localPosition_;
    float distance = toPlayer.Length();
    
    charger.timer_ += dt;

    if (charger.state_ == 0) // 照準・待機
    {
        // プレイヤーの方向を向く
        if (distance > 0.0f) {
            float targetYaw = std::atan2(toPlayer.x, toPlayer.z);
            trans.localRotation_.y = MathUtils::LerpAngle(trans.localRotation_.y, targetYaw, 0.1f);
            trans.isDirty_ = true;
        }

        // 1.5秒待機したら突進開始
        if (charger.timer_ >= 1.5f) {
            charger.state_ = 1;
            charger.timer_ = 0.0f;
            if (distance > 0.0f) {
                charger.chargeDirection_ = toPlayer;
                charger.chargeDirection_.NormalizeSelf();
            } else {
                charger.chargeDirection_ = {0.0f, 0.0f, 1.0f}; // 念のためのフォールバック
            }
        }
    }
    else if (charger.state_ == 1) // 突進中
    {
        // 高速で直線的に移動する (通常移動速度の4倍)
        float dashSpeed = status.moveSpeed_.GetValue() * 4.0f; 
        trans.localPosition_ = trans.localPosition_ + charger.chargeDirection_ * (dashSpeed * dt);
        trans.isDirty_ = true;

        // 0.8秒間突進したらクールダウンへ
        if (charger.timer_ >= 0.8f) {
            charger.state_ = 2;
            charger.timer_ = 0.0f;
        }
    }
    else if (charger.state_ == 2) // クールダウン（硬直）
    {
        // 1.5秒休んだら再び照準へ戻る
        if (charger.timer_ >= 1.5f) {
            charger.state_ = 0;
            charger.timer_ = 0.0f;
        }
    }
}

bool EnemyBehaviorSystem::IsInAttackRange(Registry& registry, EntityID entity, EnemyAIComponent& ai)
{
    // 既存の距離チェックロジック（必要に応じて使用）
    return false;
}
