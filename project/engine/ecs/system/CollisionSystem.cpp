#define NOMINMAX
#include "CollisionSystem.h"
#include <Windows.h>
#include <algorithm>
#include "engine/ecs/Registry.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/HierarchyComponent.h"
#include "engine/ecs/components/ColliderComponent.h"
#include "engine/ecs/components/CollisionLayerComponent.h"
#include "engine/ecs/components/CollisionResponseComponent.h"
#include "engine/gameobject/component/collision/CollisionAlgorithm.h"
#include "math/AABB.h"
#include "application/ecs/components/ObstacleComponent.h"
#include "engine/ecs/components/MovementComponent.h"
#include "math/VectorColorCodes.h"
#include "engine/manager/graphics/LineManager.h"
#include "math/MathUtils.h"

#include <execution>
#include <mutex>

CollisionSystem::CollisionSystem()
{
    // グリッドサイズを 10.0f に設定
    grid_ = std::make_unique<LinearSpatialHash>(10.0f);
}

void CollisionSystem::UpdatePreviousPositions(Registry& registry)
{
    if (!registry.HasComponentArray<ColliderComponent>()) return;

    auto& colliderArray = registry.GetArray<ColliderComponent>();
    auto& transformArray = registry.GetArray<TransformComponent>();

    // 並列で前フレーム位置を更新
    std::vector<uint32_t> indices(colliderArray.GetSize());
    std::iota(indices.begin(), indices.end(), 0);

    std::for_each(std::execution::par, indices.begin(), indices.end(), [&](uint32_t i) {
        EntityID entity = colliderArray.GetEntityFromDenseIndex(i);
        if (!transformArray.HasComponent(entity)) return;

        auto& collider = colliderArray.GetDataFromDenseIndex(i);
        auto& transform = transformArray.GetData(entity);
        collider.previousPosition_ = MathUtils::GetTranslateFromMatrix(transform.worldMatrix_);
    });
}

void CollisionSystem::Update(Registry& registry)
{
    // --- 1. 定期的なクレンジング ---
    if (registry.HasComponentArray<CollisionResponseComponent>())
    {
        auto& responseArray = registry.GetArray<CollisionResponseComponent>();
        for (uint32_t i = 0; i < responseArray.GetSize(); ++i) {
            responseArray.GetDataFromDenseIndex(i).ClearFrameEvents();
        }
    }

    if (registry.HasComponentArray<MovementComponent>())
    {
        auto& moveArray = registry.GetArray<MovementComponent>();
        for (uint32_t j = 0; j < moveArray.GetSize(); ++j) {
            moveArray.GetDataFromDenseIndex(j).isGrounded_ = false;
        }
    }

    if (!registry.HasComponentArray<ColliderComponent>() || !registry.HasComponentArray<TransformComponent>()) {
        return;
    }

    auto& colliderArray = registry.GetArray<ColliderComponent>();
    auto& transformArray = registry.GetArray<TransformComponent>();
    auto* layerArray = registry.HasComponentArray<CollisionLayerComponent>() ? &registry.GetArray<CollisionLayerComponent>() : nullptr;
    auto* obstacleArray = registry.HasComponentArray<ObstacleComponent>() ? &registry.GetArray<ObstacleComponent>() : nullptr;
    auto* responseArray = registry.HasComponentArray<CollisionResponseComponent>() ? &registry.GetArray<CollisionResponseComponent>() : nullptr;

    const uint32_t colliderCount = colliderArray.GetSize();
    std::vector<uint32_t> colliderIndices(colliderCount);
    std::iota(colliderIndices.begin(), colliderIndices.end(), 0);

    // --- 2. ワールド形状の更新 (Parallel) ---
    std::for_each(std::execution::par, colliderIndices.begin(), colliderIndices.end(), [&](uint32_t i) {
        EntityID entity = colliderArray.GetEntityFromDenseIndex(i);
        auto& collider = colliderArray.GetDataFromDenseIndex(i);
        if (!collider.isActive_ || !transformArray.HasComponent(entity)) return;

        auto& transform = transformArray.GetData(entity);

        // 行列構築ロジック (既存を整理)
        Matrix4x4 worldMat;
        if (transform.isDirty_) {
            Matrix4x4 localMat = MakeAffineMatrix(transform.localScale_, transform.localRotation_, transform.localPosition_);
            if (registry.HasComponent<HierarchyComponent>(entity)) {
                EntityID parentId = registry.GetComponent<HierarchyComponent>(entity).parent_;
                if (parentId != kInvalidEntity && transformArray.HasComponent(parentId)) {
                    worldMat = Multiply(localMat, transformArray.GetData(parentId).worldMatrix_);
                } else worldMat = localMat;
            } else worldMat = localMat;
        } else worldMat = transform.worldMatrix_;

        Vector3 worldPos = MathUtils::GetTranslateFromMatrix(worldMat);
        Matrix4x4 worldRot = MathUtils::GetMatrixRotate(worldMat);
        Vector3 worldScale = MathUtils::GetScaleFromMatrix(worldMat);
        Vector3 rotatedOffset = MathUtils::TransformNormal(collider.offset_, worldRot);
        Vector3 center = worldPos + rotatedOffset;

        if (collider.type_ == ColliderType::Sphere) {
            float maxS = std::max({ worldScale.x, worldScale.y, worldScale.z });
            collider.worldSphere_ = Sphere(center, collider.sphere_.radius * maxS);
            float r = collider.worldSphere_.radius;
            collider.worldAabb_ = AABB(center - Vector3(r, r, r), center + Vector3(r, r, r));
        } else if (collider.type_ == ColliderType::OBB) {
            collider.worldObb_.center = center;
            collider.worldObb_.size = worldScale * ((collider.obb_.size.LengthSquared() > 1e-6f) ? collider.obb_.size : Vector3(1, 1, 1));
            collider.worldObb_.rotate = worldRot;
            
            // OBBを包むAABBの計算
            Vector3 halfEx = {
                std::abs(worldRot.m[0][0]*collider.worldObb_.size.x) + std::abs(worldRot.m[1][0]*collider.worldObb_.size.y) + std::abs(worldRot.m[2][0]*collider.worldObb_.size.z),
                std::abs(worldRot.m[0][1]*collider.worldObb_.size.x) + std::abs(worldRot.m[1][1]*collider.worldObb_.size.y) + std::abs(worldRot.m[2][1]*collider.worldObb_.size.z),
                std::abs(worldRot.m[0][2]*collider.worldObb_.size.x) + std::abs(worldRot.m[1][2]*collider.worldObb_.size.y) + std::abs(worldRot.m[2][2]*collider.worldObb_.size.z)
            };
            collider.worldAabb_ = AABB(center - halfEx, center + halfEx);
        } else {
            Vector3 halfSize = collider.aabb_.GetHalfSize() * worldScale;
            collider.worldAabb_ = AABB(center - halfSize, center + halfSize);
        }

        if (collider.useSubstep_) {
            Vector3 prevCenter = collider.previousPosition_ + rotatedOffset;
            AABB prevAabb(prevCenter - collider.worldAabb_.GetHalfSize(), prevCenter + collider.worldAabb_.GetHalfSize());
            collider.worldAabb_.min_ = Vector3::Min(collider.worldAabb_.min_, prevAabb.min_);
            collider.worldAabb_.max_ = Vector3::Max(collider.worldAabb_.max_, prevAabb.max_);
        }
    });

    // --- 3. 空間ハッシュの再構築 (Serial Build) ---
    grid_->Clear();
    for (uint32_t i : colliderIndices) {
        auto& col = colliderArray.GetDataFromDenseIndex(i);
        if (col.isActive_) grid_->AddCount(col.worldAabb_);
    }
    grid_->BuildOffsets();
    for (uint32_t i : colliderIndices) {
        EntityID ent = colliderArray.GetEntityFromDenseIndex(i);
        auto& col = colliderArray.GetDataFromDenseIndex(i);
        if (col.isActive_) grid_->AddEntity(ent, col.worldAabb_);
    }

    // --- 4. 衝突詳細判定 & レスポンス (Parallel) ---
    struct CollisionEvent { EntityID a, b; bool isSolid; Vector3 mtv; };
    std::vector<std::vector<CollisionEvent>> threadResults; // スレッドごとに結果を収集
    std::mutex mergeMutex;

    // スレッドローカル的な代用としてエンティティごとに収集し、後でマージ
    // 実際はスレッド単位のプールが理想だが、ここでは並列ループ内で結果を蓄積する
    std::for_each(std::execution::par, colliderIndices.begin(), colliderIndices.end(), [&](uint32_t i) {
        EntityID entityA = colliderArray.GetEntityFromDenseIndex(i);
        auto& colA = colliderArray.GetDataFromDenseIndex(i);
        if (!colA.isActive_) return;

        // レイヤー取得
        uint32_t catA = 0, maskA = 0xFFFFFFFF, idxA = 0;
        if (layerArray && layerArray->HasComponent(entityA)) {
            auto& lyr = layerArray->GetData(entityA);
            catA = lyr.category_; maskA = lyr.mask_;
            if (catA & CollisionLayerComponent::kPlayer) idxA = collisionAlgorithm::kLyrPlayer;
            else if (catA & CollisionLayerComponent::kEnemy) idxA = collisionAlgorithm::kLyrEnemy;
            else if (catA & CollisionLayerComponent::kObstacle) idxA = collisionAlgorithm::kLyrObstacle;
            else if (catA & CollisionLayerComponent::kPlayerBullet) idxA = collisionAlgorithm::kLyrPlayerBullet;
            else if (catA & CollisionLayerComponent::kEnemyBullet) idxA = collisionAlgorithm::kLyrEnemyBullet;
        }

        // 近傍エンティティの重複排除用
        std::vector<EntityID> neighbors;
        grid_->QueryNearby(colA.worldAabb_, [&](EntityID neighbor) {
            if (entityA < neighbor) neighbors.push_back(neighbor);
        });
        std::sort(neighbors.begin(), neighbors.end());
        neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());

        std::vector<CollisionEvent> localResults;

        for (EntityID entityB : neighbors) {
            if (!registry.IsAlive(entityB) || !colliderArray.HasComponent(entityB)) continue;
            auto& colB = colliderArray.GetData(entityB);
            if (!colB.isActive_) continue;

            // 早期フィルタリング
            uint32_t catB = 0, maskB = 0xFFFFFFFF, idxB = 0;
            if (layerArray && layerArray->HasComponent(entityB)) {
                auto& lyr = layerArray->GetData(entityB);
                catB = lyr.category_; maskB = lyr.mask_;
                if (catB & CollisionLayerComponent::kPlayer) idxB = collisionAlgorithm::kLyrPlayer;
                else if (catB & CollisionLayerComponent::kEnemy) idxB = collisionAlgorithm::kLyrEnemy;
                else if (catB & CollisionLayerComponent::kObstacle) idxB = collisionAlgorithm::kLyrObstacle;
                else if (catB & CollisionLayerComponent::kPlayerBullet) idxB = collisionAlgorithm::kLyrPlayerBullet;
                else if (catB & CollisionLayerComponent::kEnemyBullet) idxB = collisionAlgorithm::kLyrEnemyBullet;
            }
            if (!(catA & maskB) || !(catB & maskA)) continue;

            // 障害物同士はスキップ
            bool isAObs = obstacleArray && obstacleArray->HasComponent(entityA);
            bool isBObs = obstacleArray && obstacleArray->HasComponent(entityB);
            if (isAObs && isBObs) continue;

            // LOD Matrix から判定レベルを取得
            collisionAlgorithm::CollisionLOD lod = collisionAlgorithm::kLODMatrix[idxA][idxB];
            
            bool hit = false;
            Vector3 mtv = {0,0,0};
            bool needsMTV = (!colA.isTrigger_ && !colB.isTrigger_) && (isAObs || isBObs);

            // 関数テーブルによるディスパッチ
            auto func = (lod == collisionAlgorithm::CollisionLOD::CCD) ? collisionAlgorithm::kCCDFuncTable[(int)colA.type_][(int)colB.type_] : collisionAlgorithm::kCollisionFuncTable[(int)colA.type_][(int)colB.type_];
            if (!func) func = collisionAlgorithm::kCollisionFuncTable[(int)colA.type_][(int)colB.type_]; // Fallback

            if (func) {
                if (lod == collisionAlgorithm::CollisionLOD::Sphere) hit = collisionAlgorithm::CheckSpherevsSphere(colA.worldSphere_, colB.worldSphere_);
                else hit = func(colA, colB, needsMTV ? &mtv : nullptr);
            }

            if (hit) {
                localResults.push_back({ entityA, entityB, needsMTV, mtv });
            }
        }

        if (!localResults.empty()) {
            std::lock_guard<std::mutex> lock(mergeMutex);
            threadResults.push_back(std::move(localResults));
        }
    });

    // --- 5. 結果のマージとレスポンス (Sequential) ---
    for (const auto& batch : threadResults) {
        for (const auto& ev : batch) {
            // イベント記録
            if (responseArray) {
                if (responseArray->HasComponent(ev.a)) responseArray->GetData(ev.a).currentCollisions_.insert(ev.b);
                if (responseArray->HasComponent(ev.b)) responseArray->GetData(ev.b).currentCollisions_.insert(ev.a);
            }

            // 押し戻し (Solid Response)
            if (ev.isSolid) {
                bool isAObs = obstacleArray && obstacleArray->HasComponent(ev.a);
                EntityID charEnt = isAObs ? ev.b : ev.a;
                Vector3 finalMtv = isAObs ? ev.mtv : -ev.mtv; // ev.mtv は BをAから遠ざける方向

                auto& trans = transformArray.GetData(charEnt);
                Vector3 currentPos = MathUtils::GetTranslateFromMatrix(trans.worldMatrix_);
                Vector3 newWorldPos = currentPos + finalMtv;

                trans.worldMatrix_.m[3][0] = newWorldPos.x;
                trans.worldMatrix_.m[3][1] = newWorldPos.y;
                trans.worldMatrix_.m[3][2] = newWorldPos.z;
                
                // 親子関係考慮のローカル座標変換は簡略化（必要ならここで行う）
                trans.localPosition_ = newWorldPos; 
                trans.isDirty_ = true;

                if (registry.HasComponent<MovementComponent>(charEnt) && finalMtv.y > 0.05f) {
                    registry.GetComponent<MovementComponent>(charEnt).isGrounded_ = true;
                }
            }

            // 特殊ロジック（Player vs Enemy）
            // 既存の registry.DestroyEntityDeferred はメインスレッドで安全
            uint32_t catA = layerArray->HasComponent(ev.a) ? layerArray->GetData(ev.a).category_ : 0;
            uint32_t catB = layerArray->HasComponent(ev.b) ? layerArray->GetData(ev.b).category_ : 0;
            if ((catA & CollisionLayerComponent::kPlayer) && (catB & CollisionLayerComponent::kEnemy)) registry.DestroyEntityDeferred(ev.b);
            else if ((catB & CollisionLayerComponent::kPlayer) && (catA & CollisionLayerComponent::kEnemy)) registry.DestroyEntityDeferred(ev.a);
        }
    }



    // 前フレーム記録は UpdatePreviousPositions へ移動

    // --- 5. 衝突イベントコールバック ---
    if (registry.HasComponentArray<CollisionResponseComponent>()) {
        auto& responseArray = registry.GetArray<CollisionResponseComponent>();
        for (uint32_t i = 0; i < responseArray.GetSize(); ++i) {
            EntityID entity = responseArray.GetEntityFromDenseIndex(i);
            auto& res = responseArray.GetDataFromDenseIndex(i);
            bool hasColl = registry.HasComponent<ColliderComponent>(entity);
            
            for (EntityID current : res.currentCollisions_) {
                if (res.previousCollisions_.count(current)) {
                    if (hasColl) { auto& c = registry.GetComponent<ColliderComponent>(entity); if (c.onStay_) c.onStay_(current); }
                } else {
                    if (hasColl) { auto& c = registry.GetComponent<ColliderComponent>(entity); if (c.onEnter_) c.onEnter_(current); }
                }
            }
            for (EntityID prev : res.previousCollisions_) {
                if (!res.currentCollisions_.count(prev)) {
                    if (hasColl) { auto& c = registry.GetComponent<ColliderComponent>(entity); if (c.onExit_) c.onExit_(prev); }
                }
            }
        }
    }
}

void CollisionSystem::Draw(Registry& registry, Camera* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager)
{
#ifdef _DEBUG
    auto* lineManager = LineManager::GetInstance();
    if (!lineManager) return;

    // --- 1. コライダーの描画 (既存) ---
    if (registry.HasComponentArray<ColliderComponent>()) {
        auto& colliderArray = registry.GetArray<ColliderComponent>();
        for (uint32_t i = 0; i < colliderArray.GetSize(); ++i) {
            EntityID entity = colliderArray.GetEntityFromDenseIndex(i);
            auto& collider = colliderArray.GetDataFromDenseIndex(i);
            if (!collider.isActive_ || !registry.HasComponent<TransformComponent>(entity)) continue;

            Vector4 color = collider.isTrigger_ ? VectorColorCodes::Lime : VectorColorCodes::Cyan;
            switch (collider.type_) {
            case ColliderType::AABB:   lineManager->DrawAABB(collider.worldAabb_, color); break;
            case ColliderType::Sphere: lineManager->DrawSphere(collider.worldSphere_.center, collider.worldSphere_.radius, color); break;
            case ColliderType::OBB:    lineManager->DrawOBB(collider.worldObb_, color); break;
            }
        }
    }

    // --- 2. 空間ハッシュヒートマップの描画 (新規) ---
    // デバッグ用に負荷（密度）の高いセルを赤く表示する
    // 本来はグリッドの範囲が必要だが、ここではコライダーが存在する範囲のみを走査して描画する
    if (registry.HasComponentArray<ColliderComponent>()) {
        auto& colliderArray = registry.GetArray<ColliderComponent>();
        for (uint32_t i = 0; i < colliderArray.GetSize(); ++i) {
            auto& col = colliderArray.GetDataFromDenseIndex(i);
            if (!col.isActive_) continue;

            // コライダーが触れている全バケットを可視化
            // ※IterateCells は LinearSpatialHash の private なので、
            //   ここでは簡易的に AABB の中心付近のバケット等を表示するか、
            //   あるいは GetBucketCount を利用して近傍を描画する
            
            // 簡易ヒートマップ：AABBを 10.0f (cellSize) ごとに走査
            float step = 10.0f; // grid_->cellSize_ と合わせる
            for (float x = std::floor(col.worldAabb_.min_.x / step) * step; x <= col.worldAabb_.max_.x; x += step) {
                for (float z = std::floor(col.worldAabb_.min_.z / step) * step; z <= col.worldAabb_.max_.z; z += step) {
                    // Yは地面付近（0）に固定して描画
                    Vector3 minP = { x, 0.0f, z };
                    Vector3 maxP = { x + step, 0.1f, z + step };
                    
                    // 密度に応じた色（暫定）
                    // 本来はバケットの count を GetHash で引いて色を変えるべきだが、
                    // ここでは「存在レイヤー」の可視化として機能させる
                    lineManager->DrawAABB(AABB(minP, maxP), { 1.0f, 0.0f, 0.0f, 0.3f }); 
                }
            }
        }
    }
#endif
}
