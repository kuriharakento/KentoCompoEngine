#define NOMINMAX
#include "CollisionSystem.h"
#include <Windows.h>
#include <algorithm>
#include "engine/ecs/Registry.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/ColliderComponent.h"
#include "engine/ecs/components/CollisionLayerComponent.h"
#include "engine/ecs/components/CollisionResponseComponent.h"
#include "engine/gameobject/component/collision/CollisionAlgorithm.h"
#include "math/AABB.h"
#include "application/ecs/components/ObstacleComponent.h"
#include "engine/ecs/components/MovementComponent.h"
#include "math/VectorColorCodes.h"
#include "engine/manager/graphics/LineManager.h"

CollisionSystem::CollisionSystem()
{
    // グリッドサイズを 10.0f に設定
    grid_ = std::make_unique<SpatialGrid>(10.0f);
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

    // --- 2. ワールド形状の更新 (判定と描画の同期) ---
    for (uint32_t i = 0; i < colliderArray.GetSize(); ++i) {
        EntityID entity = colliderArray.GetEntityFromDenseIndex(i);
        auto& collider = colliderArray.GetDataFromDenseIndex(i);
        if (!collider.isActive_ || !registry.HasComponent<TransformComponent>(entity)) continue;

        const auto& transform = registry.GetComponent<TransformComponent>(entity);
        Vector3 pos = { transform.worldMatrix_.m[3][0], transform.worldMatrix_.m[3][1], transform.worldMatrix_.m[3][2] };
        Vector3 center = pos + collider.offset_;
        Vector3 scale = transform.localScale_;

        if (collider.type_ == ColliderType::AABB) {
            Vector3 halfSize = collider.aabb_.GetHalfSize() * scale;
            collider.worldAabb_ = AABB(center - halfSize, center + halfSize);
        } else if (collider.type_ == ColliderType::Sphere) {
            float maxS = std::max({ scale.x, scale.y, scale.z });
            collider.worldSphere_ = Sphere(center, collider.sphere_.radius * maxS);
        } else if (collider.type_ == ColliderType::OBB) {
            collider.worldObb_ = collider.obb_;
            collider.worldObb_.center = center;
            collider.worldObb_.size = collider.obb_.size * scale;
            
            // ワールド行列から回転を抽出
            Matrix4x4 rot = MakeIdentity4x4();
            Vector3 axX = { transform.worldMatrix_.m[0][0], transform.worldMatrix_.m[0][1], transform.worldMatrix_.m[0][2] };
            Vector3 axY = { transform.worldMatrix_.m[1][0], transform.worldMatrix_.m[1][1], transform.worldMatrix_.m[1][2] };
            Vector3 axZ = { transform.worldMatrix_.m[2][0], transform.worldMatrix_.m[2][1], transform.worldMatrix_.m[2][2] };
            axX.Normalize(); axY.Normalize(); axZ.Normalize();
            rot.m[0][0] = axX.x; rot.m[0][1] = axX.y; rot.m[0][2] = axX.z;
            rot.m[1][0] = axY.x; rot.m[1][1] = axY.y; rot.m[1][2] = axY.z;
            rot.m[2][0] = axZ.x; rot.m[2][1] = axZ.y; rot.m[2][2] = axZ.z;
            collider.worldObb_.rotate = rot;
        }
    }

    // --- 3. 空間分割グリッドの構築 ---
    grid_->Clear();
    for (uint32_t i = 0; i < colliderArray.GetSize(); ++i) {
        EntityID entity = colliderArray.GetEntityFromDenseIndex(i);
        auto& collider = colliderArray.GetDataFromDenseIndex(i);
        if (!collider.isActive_) continue;

        AABB gridBounds;
        if (collider.type_ == ColliderType::AABB) gridBounds = collider.worldAabb_;
        else if (collider.type_ == ColliderType::Sphere) {
            float r = collider.worldSphere_.radius;
            gridBounds = AABB(collider.worldSphere_.center - Vector3(r,r,r), collider.worldSphere_.center + Vector3(r,r,r));
        } else {
            float r = collider.worldObb_.size.Length();
            gridBounds = AABB(collider.worldObb_.center - Vector3(r,r,r), collider.worldObb_.center + Vector3(r,r,r));
        }
        grid_->Add(entity, gridBounds);
    }

    // --- 4. 衝突詳細判定 & レスポンス ---
    std::vector<EntityID> neighbors;
    for (uint32_t i = 0; i < colliderArray.GetSize(); ++i) {
        EntityID entityA = colliderArray.GetEntityFromDenseIndex(i);
        auto& colliderA = colliderArray.GetDataFromDenseIndex(i);
        if (!colliderA.isActive_) continue;

        AABB queryAABB;
        if (colliderA.type_ == ColliderType::AABB) queryAABB = colliderA.worldAabb_;
        else if (colliderA.type_ == ColliderType::Sphere) {
            float r = colliderA.worldSphere_.radius;
            queryAABB = AABB(colliderA.worldSphere_.center - Vector3(r,r,r), colliderA.worldSphere_.center + Vector3(r,r,r));
        } else {
            float r = colliderA.worldObb_.size.Length();
            queryAABB = AABB(colliderA.worldObb_.center - Vector3(r,r,r), colliderA.worldObb_.center + Vector3(r,r,r));
        }

        neighbors.clear();
        grid_->GetNearbyEntities(queryAABB, neighbors);

        for (EntityID entityB : neighbors) {
            if (entityA >= entityB) continue;
            
            auto& colliderB = registry.GetComponent<ColliderComponent>(entityB);
            if (!colliderB.isActive_) continue;

            // レイヤマスクによるフィルタリング
            uint32_t categoryA = 0, maskA = 0xFFFFFFFF;
            if (registry.HasComponent<CollisionLayerComponent>(entityA)) {
                categoryA = registry.GetComponent<CollisionLayerComponent>(entityA).category_;
                maskA = registry.GetComponent<CollisionLayerComponent>(entityA).mask_;
            }
            uint32_t categoryB = 0, maskB = 0xFFFFFFFF;
            if (registry.HasComponent<CollisionLayerComponent>(entityB)) {
                categoryB = registry.GetComponent<CollisionLayerComponent>(entityB).category_;
                maskB = registry.GetComponent<CollisionLayerComponent>(entityB).mask_;
            }
            if (!(categoryA & maskB) && !(categoryB & maskA)) continue;

            bool isColliding = false;
            bool useSubstep = colliderA.useSubstep_ || colliderB.useSubstep_;

            if (colliderA.type_ == ColliderType::AABB && colliderB.type_ == ColliderType::AABB) {
                if (useSubstep) isColliding = collisionAlgorithm::CheckAABBvsAABBSubstep(colliderA.worldAabb_, colliderA.previousPosition_, colliderB.worldAabb_, colliderB.previousPosition_);
                else isColliding = collisionAlgorithm::CheckAABBvsAABB(colliderA.worldAabb_, colliderB.worldAabb_);
            }
            else if (colliderA.type_ == ColliderType::Sphere && colliderB.type_ == ColliderType::Sphere) {
                if (useSubstep) isColliding = collisionAlgorithm::CheckSpherevsSphereSubstep(colliderA.worldSphere_, colliderA.previousPosition_, colliderB.worldSphere_, colliderB.previousPosition_);
                else isColliding = collisionAlgorithm::CheckSpherevsSphere(colliderA.worldSphere_, colliderB.worldSphere_);
            }
            else if (colliderA.type_ == ColliderType::AABB && colliderB.type_ == ColliderType::Sphere) {
                if (useSubstep) isColliding = collisionAlgorithm::CheckSpherevsAABBSubstep(colliderB.worldSphere_, colliderB.previousPosition_, colliderA.worldAabb_, colliderA.previousPosition_);
                else isColliding = collisionAlgorithm::CheckSpherevsAABB(colliderB.worldSphere_, colliderA.worldAabb_);
            }
            else if (colliderA.type_ == ColliderType::Sphere && colliderB.type_ == ColliderType::AABB) {
                if (useSubstep) isColliding = collisionAlgorithm::CheckSpherevsAABBSubstep(colliderA.worldSphere_, colliderA.previousPosition_, colliderB.worldAabb_, colliderB.previousPosition_);
                else isColliding = collisionAlgorithm::CheckSpherevsAABB(colliderA.worldSphere_, colliderB.worldAabb_);
            }
            else if (colliderA.type_ == ColliderType::OBB && colliderB.type_ == ColliderType::OBB) {
                isColliding = collisionAlgorithm::CheckOBBvsOBB(colliderA.worldObb_, colliderB.worldObb_);
            }
            else if (colliderA.type_ == ColliderType::AABB && colliderB.type_ == ColliderType::OBB) {
                isColliding = collisionAlgorithm::CheckAABBvsOBB(colliderA.worldAabb_, colliderB.worldObb_);
            }
            else if (colliderA.type_ == ColliderType::OBB && colliderB.type_ == ColliderType::AABB) {
                isColliding = collisionAlgorithm::CheckAABBvsOBB(colliderB.worldAabb_, colliderA.worldObb_);
            }
            else if (colliderA.type_ == ColliderType::Sphere && colliderB.type_ == ColliderType::OBB) {
                isColliding = collisionAlgorithm::CheckSpherevsOBB(colliderA.worldSphere_, colliderB.worldObb_);
            }
            else if (colliderA.type_ == ColliderType::OBB && colliderB.type_ == ColliderType::Sphere) {
                isColliding = collisionAlgorithm::CheckSpherevsOBB(colliderB.worldSphere_, colliderA.worldObb_);
            }

            if (isColliding) {
                if (registry.HasComponent<CollisionResponseComponent>(entityA))
                    registry.GetComponent<CollisionResponseComponent>(entityA).currentCollisions_.insert(entityB);
                if (registry.HasComponent<CollisionResponseComponent>(entityB))
                    registry.GetComponent<CollisionResponseComponent>(entityB).currentCollisions_.insert(entityA);

                // 物理的な解決 (押し戻し)
                if (!colliderA.isTrigger_ && !colliderB.isTrigger_) {
                    bool isAObs = registry.HasComponent<ObstacleComponent>(entityA);
                    bool isBObs = registry.HasComponent<ObstacleComponent>(entityB);

                    if (isAObs || isBObs) {
                        EntityID charEnt = isAObs ? entityB : entityA;
                        EntityID obstEnt = isAObs ? entityA : entityB;
                        
                        auto& charTrans = registry.GetComponent<TransformComponent>(charEnt);
                        auto& charColl = registry.GetComponent<ColliderComponent>(charEnt);
                        const auto& obstColl = registry.GetComponent<ColliderComponent>(obstEnt);

                        Vector3 mtv = {0,0,0};
                        bool solved = false;

                        if (charColl.type_ == ColliderType::Sphere) {
                            if (obstColl.type_ == ColliderType::OBB) solved = collisionAlgorithm::CheckSpherevsOBBMTV(charColl.worldSphere_, obstColl.worldObb_, mtv);
                            else if (obstColl.type_ == ColliderType::AABB) solved = collisionAlgorithm::CheckSpherevsAABBMTV(charColl.worldSphere_, obstColl.worldAabb_, mtv);
                            else if (obstColl.type_ == ColliderType::Sphere) solved = collisionAlgorithm::CheckSpherevsSphereMTV(charColl.worldSphere_, obstColl.worldSphere_, mtv);
                        } 
                        else if (charColl.type_ == ColliderType::OBB) {
                            if (obstColl.type_ == ColliderType::OBB) solved = collisionAlgorithm::CheckOBBvsOBBMTV(charColl.worldObb_, obstColl.worldObb_, mtv);
                            else if (obstColl.type_ == ColliderType::AABB) {
                                OBB oB; 
                                oB.center = obstColl.worldAabb_.GetCenter();
                                oB.size = obstColl.worldAabb_.GetHalfSize();
                                oB.rotate = MakeIdentity4x4();
                                solved = collisionAlgorithm::CheckOBBvsOBBMTV(charColl.worldObb_, oB, mtv);
                            }
                        }
                        else if (charColl.type_ == ColliderType::AABB) {
                            if (obstColl.type_ == ColliderType::AABB) solved = collisionAlgorithm::CheckAABBvsAABBMTV(charColl.worldAabb_, obstColl.worldAabb_, mtv);
                            else if (obstColl.type_ == ColliderType::OBB) {
                                OBB oA;
                                oA.center = charColl.worldAabb_.GetCenter();
                                oA.size = charColl.worldAabb_.GetHalfSize();
                                oA.rotate = MakeIdentity4x4();
                                solved = collisionAlgorithm::CheckOBBvsOBBMTV(oA, obstColl.worldObb_, mtv);
                            }
                        }

                        if (solved) {
                            charTrans.localPosition_ += mtv;
                            charTrans.isDirty_ = true;
                            // 行列の平行移動成分を即時更新
                            charTrans.worldMatrix_.m[3][0] += mtv.x;
                            charTrans.worldMatrix_.m[3][1] += mtv.y;
                            charTrans.worldMatrix_.m[3][2] += mtv.z;
                            
                            // 更新後の座標を保存用worldShapeに反映（次ペア用）
                            if (charColl.type_ == ColliderType::AABB) { charColl.worldAabb_.min_ += mtv; charColl.worldAabb_.max_ += mtv; }
                            if (charColl.type_ == ColliderType::Sphere) charColl.worldSphere_.center += mtv;
                            if (charColl.type_ == ColliderType::OBB) charColl.worldObb_.center += mtv;

                            if (registry.HasComponent<MovementComponent>(charEnt)) {
                                if (mtv.y > 0.05f) registry.GetComponent<MovementComponent>(charEnt).isGrounded_ = true;
                            }
                        }
                    }
                }
            }
        }
    }

    // 前フレーム位置を記録 (Updateの最後)
    for (uint32_t i = 0; i < colliderArray.GetSize(); ++i) {
        EntityID entity = colliderArray.GetEntityFromDenseIndex(i);
        auto& collider = colliderArray.GetDataFromDenseIndex(i);
        if (!collider.isActive_) continue;
        const auto& transform = registry.GetComponent<TransformComponent>(entity);
        collider.previousPosition_ = { transform.worldMatrix_.m[3][0], transform.worldMatrix_.m[3][1], transform.worldMatrix_.m[3][2] };
    }

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
    if (!registry.HasComponentArray<ColliderComponent>()) return;

    auto& colliderArray = registry.GetArray<ColliderComponent>();
    auto* lineManager = LineManager::GetInstance();
    
    for (uint32_t i = 0; i < colliderArray.GetSize(); ++i) {
        auto& collider = colliderArray.GetDataFromDenseIndex(i);
        if (!collider.isActive_) continue;

        Vector4 colorTrigger = VectorColorCodes::Lime;
        Vector4 colorSolid = VectorColorCodes::Cyan;
        Vector4 color = collider.isTrigger_ ? colorTrigger : colorSolid;

        switch (collider.type_) {
        case ColliderType::AABB:
            lineManager->DrawAABB(collider.worldAabb_, color);
            break;
        case ColliderType::Sphere:
            lineManager->DrawSphere(collider.worldSphere_.center, collider.worldSphere_.radius, color);
            break;
        case ColliderType::OBB:
            lineManager->DrawOBB(collider.worldObb_, color);
            break;
        }
    }
#endif
}
