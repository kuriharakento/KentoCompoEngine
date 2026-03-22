#include "CollisionSystem.h"
#include "engine/ecs/Registry.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/ColliderComponent.h"
#include "engine/ecs/components/CollisionLayerComponent.h"
#include "engine/ecs/components/MovementComponent.h"

CollisionSystem::CollisionSystem()
{
    // グリッドサイズを 10.0 程度に設定（要調整）
    grid_ = std::make_unique<SpatialGrid>(10.0f);
}

void CollisionSystem::Update(Registry& registry)
{
    if (!registry.HasComponentArray<ColliderComponent>()) return;

    auto colliderView = registry.View<ColliderComponent>();
    if (!colliderView) return;

    // 1. グリッドの構築
    grid_->Clear();
    for (uint32_t i = 0; i < colliderView->GetSize(); ++i)
    {
        EntityID entity = colliderView->GetEntityFromDenseIndex(i);
        if (!registry.HasComponent<TransformComponent>(entity)) continue;

        auto& transform = registry.GetComponent<TransformComponent>(entity);
        grid_->Add(entity, transform.localPosition_);
    }

    // 2. 判定と応答（簡易的な実装）
    std::vector<EntityID> neighbors;
    for (uint32_t i = 0; i < colliderView->GetSize(); ++i)
    {
        EntityID entityA = colliderView->GetEntityFromDenseIndex(i);
        auto& colA = colliderView->GetData(entityA);
        if (!colA.isActive_) continue;
        
        auto& transA = registry.GetComponent<TransformComponent>(entityA);
        
        neighbors.clear();
        grid_->GetNearbyEntities(transA.localPosition_, neighbors);

        for (EntityID entityB : neighbors)
        {
            if (entityA == entityB) continue;

            auto& colB = registry.GetComponent<ColliderComponent>(entityB);
            if (!colB.isActive_) continue;

            // TODO: レイヤーによるフィルタリング（CollisionLayerComponent があれば）
            
            // 簡易的な AABB 判定
            AABB worldA = colA.aabb_;
            worldA.min_.x += transA.localPosition_.x + colA.offset_.x;
            worldA.min_.y += transA.localPosition_.y + colA.offset_.y;
            worldA.min_.z += transA.localPosition_.z + colA.offset_.z;
            worldA.max_.x += transA.localPosition_.x + colA.offset_.x;
            worldA.max_.y += transA.localPosition_.y + colA.offset_.y;
            worldA.max_.z += transA.localPosition_.z + colA.offset_.z;

            auto& transB = registry.GetComponent<TransformComponent>(entityB);
            AABB worldB = colB.aabb_;
            worldB.min_.x += transB.localPosition_.x + colB.offset_.x;
            worldB.min_.y += transB.localPosition_.y + colB.offset_.y;
            worldB.min_.z += transB.localPosition_.z + colB.offset_.z;
            worldB.max_.x += transB.localPosition_.x + colB.offset_.x;
            worldB.max_.y += transB.localPosition_.y + colB.offset_.y;
            worldB.max_.z += transB.localPosition_.z + colB.offset_.z;

            // AABB vs AABB
            if (worldA.min_.x < worldB.max_.x && worldA.max_.x > worldB.min_.x &&
                worldA.min_.y < worldB.max_.y && worldA.max_.y > worldB.min_.y &&
                worldA.min_.z < worldB.max_.z && worldA.max_.z > worldB.min_.z)
            {
                // 衝突！
                // 押し返し処理（簡易的：MovementComponent があれば）
                if (!colA.isTrigger_ && !colB.isTrigger_)
                {
                    if (registry.HasComponent<MovementComponent>(entityA))
                    {
                        auto& moveA = registry.GetComponent<MovementComponent>(entityA);
                        // とりあえず重なりを解消するように微調整するか、速度を反転・減衰させる
                        // ここは後で本格的な物理応答を入れる（今は移動停止など）
                        // moveA.velocity_ = { 0, 0, 0 };
                    }
                }
            }
        }
    }
}
