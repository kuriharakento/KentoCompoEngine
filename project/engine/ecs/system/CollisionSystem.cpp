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

CollisionSystem::CollisionSystem()
{
    // グリッドサイズを 10.0f に設定
    grid_ = std::make_unique<SpatialGrid>(10.0f);
}

void CollisionSystem::UpdatePreviousPositions(Registry& registry)
{
    if (!registry.HasComponentArray<ColliderComponent>() || !registry.HasComponentArray<TransformComponent>()) {
        return;
    }

    auto& colliderArray = registry.GetArray<ColliderComponent>();
    for (uint32_t i = 0; i < colliderArray.GetSize(); ++i) {
        EntityID entity = colliderArray.GetEntityFromDenseIndex(i);
        auto& collider = colliderArray.GetDataFromDenseIndex(i);
        
        if (!registry.HasComponent<TransformComponent>(entity)) continue;

        // worldShape の中心を前フレームの位置として記録
        // サブステップ判定は worldObb_.center を使うため座標系を合わせる
        switch (collider.type_) {
            case ColliderType::OBB:    collider.previousPosition_ = collider.worldObb_.center;       break;
            case ColliderType::AABB:   collider.previousPosition_ = collider.worldAabb_.GetCenter(); break;
            case ColliderType::Sphere: collider.previousPosition_ = collider.worldSphere_.center;    break;
            default: {
                const auto& t = registry.GetComponent<TransformComponent>(entity);
                collider.previousPosition_ = t.localPosition_;
                break;
            }
        }
    }
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

    // --- 2. ワールド形状の更新 ---
    // isDirty_ = true  → MovementSystem/PlayerSystem が位置を変更した動的エンティティ
    //                    worldMatrix_ が stale なので localPos/Rot/Scale から新鮮な行列を構築する
    // isDirty_ = false → 位置が変わっていない静的エンティティ（障害物など）
    //                    HierarchySystem が構築済みの worldMatrix_ をそのまま使う
    for (uint32_t i = 0; i < colliderArray.GetSize(); ++i) {
        EntityID entity = colliderArray.GetEntityFromDenseIndex(i);
        auto& collider = colliderArray.GetDataFromDenseIndex(i);
        if (!collider.isActive_ || !registry.HasComponent<TransformComponent>(entity)) continue;

        const auto& transform = registry.GetComponent<TransformComponent>(entity);

        // isDirty_ で行列取得の戦略を切り替える
        Matrix4x4 worldMat;
        Matrix4x4 parentWorldMat = MakeIdentity4x4();
        bool hasParent = false;

        // 親エンティティの行列を取得してワールド行列を正しく構成
        if (registry.HasComponent<HierarchyComponent>(entity)) {
            EntityID parentId = registry.GetComponent<HierarchyComponent>(entity).parent_;
            if (parentId != kInvalidEntity && registry.HasComponent<TransformComponent>(parentId)) {
                parentWorldMat = registry.GetComponent<TransformComponent>(parentId).worldMatrix_;
                hasParent = true;
            }
        }

        if (transform.isDirty_) {
            // 動的エンティティ: HierarchySystem より後に位置が変わったため直接構築
            Matrix4x4 localMat = MakeAffineMatrix(transform.localScale_, transform.localRotation_, transform.localPosition_);
            if (hasParent) {
                worldMat = Multiply(localMat, parentWorldMat);
            } else {
                worldMat = localMat;
            }
        } else {
            // 静的エンティティ: HierarchySystem 時点で確定済みのキャッシュを利用
            worldMat = transform.worldMatrix_;
        }

        Vector3 worldPos   = MathUtils::GetTranslateFromMatrix(worldMat);
        Matrix4x4 worldRot = MathUtils::GetMatrixRotate(worldMat);
        Vector3 worldScale = MathUtils::GetScaleFromMatrix(worldMat);

        Vector3 rotatedOffset = MathUtils::TransformNormal(collider.offset_, worldRot);
        Vector3 center        = worldPos + rotatedOffset;

        if (collider.type_ == ColliderType::AABB) {
            Vector3 halfSize = collider.aabb_.GetHalfSize() * worldScale;
            collider.worldAabb_ = AABB(center - halfSize, center + halfSize);
        } else if (collider.type_ == ColliderType::Sphere) {
            float maxS = std::max({ worldScale.x, worldScale.y, worldScale.z });
            float r = collider.sphere_.radius * maxS;
            collider.worldSphere_ = Sphere(center, r);
            collider.worldAabb_ = AABB(center - Vector3(r, r, r), center + Vector3(r, r, r));
        } else if (collider.type_ == ColliderType::OBB) {
            collider.worldObb_.center = center;
            // ローカルサイズが設定されていない(0,0,0)場合は worldScale をそのまま使う
            Vector3 baseSize = (collider.obb_.size.LengthSquared() > 1e-6f) ? collider.obb_.size : Vector3(1.0f, 1.0f, 1.0f);
            collider.worldObb_.size   = worldScale * baseSize;
            collider.worldObb_.rotate = worldRot;

            // OBBを包むAABBを計算
            Vector3 axes[3] = {
                Vector3(worldRot.m[0][0], worldRot.m[0][1], worldRot.m[0][2]),
                Vector3(worldRot.m[1][0], worldRot.m[1][1], worldRot.m[1][2]),
                Vector3(worldRot.m[2][0], worldRot.m[2][1], worldRot.m[2][2])
            };
            Vector3 halfExtents = {
                std::abs(Vector3::Dot(axes[0]*collider.worldObb_.size.x, {1,0,0})) + std::abs(Vector3::Dot(axes[1]*collider.worldObb_.size.y, {1,0,0})) + std::abs(Vector3::Dot(axes[2]*collider.worldObb_.size.z, {1,0,0})),
                std::abs(Vector3::Dot(axes[0]*collider.worldObb_.size.x, {0,1,0})) + std::abs(Vector3::Dot(axes[1]*collider.worldObb_.size.y, {0,1,0})) + std::abs(Vector3::Dot(axes[2]*collider.worldObb_.size.z, {0,1,0})),
                std::abs(Vector3::Dot(axes[0]*collider.worldObb_.size.x, {0,0,1})) + std::abs(Vector3::Dot(axes[1]*collider.worldObb_.size.y, {0,0,1})) + std::abs(Vector3::Dot(axes[2]*collider.worldObb_.size.z, {0,0,1}))
            };
            collider.worldAabb_ = AABB(center - halfExtents, center + halfExtents);
        }
    }

    // --- 3. 衝突詳細判定 & レスポンス (空間分割なし) ---
    uint32_t colliderCount = colliderArray.GetSize();

    // 解決時の逆行列計算をキャッシュするための変数
    EntityID lastInvEntity = kInvalidEntity;
    Matrix4x4 cachedInvMat = MakeIdentity4x4();

    for (uint32_t i = 0; i < colliderCount; ++i) {
        EntityID entityA = colliderArray.GetEntityFromDenseIndex(i);
        auto& colliderA = colliderArray.GetDataFromDenseIndex(i);
        if (!colliderA.isActive_ || !registry.HasComponent<TransformComponent>(entityA)) continue;

        for (uint32_t j = i + 1; j < colliderCount; ++j) {
            EntityID entityB = colliderArray.GetEntityFromDenseIndex(j);
            auto& colliderB = colliderArray.GetDataFromDenseIndex(j);
            if (!colliderB.isActive_ || !registry.HasComponent<TransformComponent>(entityB)) continue;

            // 障害物同士（静的オブジェクト同士）の判定はスキップする
            bool isAObs = registry.HasComponent<ObstacleComponent>(entityA);
            bool isBObs = registry.HasComponent<ObstacleComponent>(entityB);
            if (isAObs && isBObs) continue;

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

            // --- ブロードフェーズ: AABBによる高速判定 ---
            if (!collisionAlgorithm::CheckAABBvsAABB(colliderA.worldAabb_, colliderB.worldAabb_)) continue;

            bool isColliding = false;
            Vector3 mtv = {0,0,0};
            bool useSubstep = colliderA.useSubstep_ || colliderB.useSubstep_;

            // 物理的な押し戻しが必要かチェック
            bool needsMTVSolve = (!colliderA.isTrigger_ && !colliderB.isTrigger_) && (isAObs || isBObs);

            if (needsMTVSolve) {
                // --- 物理的な解決 (MTV直接計算) ---
                EntityID charEnt = isAObs ? entityB : entityA;
                EntityID obstEnt = isAObs ? entityA : entityB;
                auto& charColl = registry.GetComponent<ColliderComponent>(charEnt);
                const auto& obstColl = registry.GetComponent<ColliderComponent>(obstEnt);
                const Vector3& charPrevPos = charColl.previousPosition_;
                const Vector3& obstPrevPos = obstColl.previousPosition_;

                bool solved = false;
                if (charColl.type_ == ColliderType::Sphere) {
                    if (obstColl.type_ == ColliderType::Sphere) {
                        solved = collisionAlgorithm::CheckSpherevsSphereMTV(charColl.worldSphere_, obstColl.worldSphere_, mtv);
                        if (solved) mtv = -mtv; // 第2引数(障害物)を押し出す方向で返るため反転
                    }
                    else if (obstColl.type_ == ColliderType::OBB) {
                        if (useSubstep) solved = collisionAlgorithm::CheckSpherevsOBBSubstepMTV(charColl.worldSphere_, charPrevPos, obstColl.worldObb_, obstPrevPos, mtv);
                        else solved = collisionAlgorithm::CheckSpherevsOBBMTV(charColl.worldSphere_, obstColl.worldObb_, mtv);
                        if (solved) mtv = -mtv; // 第2引数(障害物)を押し出す方向で返るため反転
                    }
                    else if (obstColl.type_ == ColliderType::AABB) {
                        if (useSubstep) solved = collisionAlgorithm::CheckSpherevsAABBSubstepMTV(charColl.worldSphere_, charPrevPos, obstColl.worldAabb_, obstPrevPos, mtv);
                        else solved = collisionAlgorithm::CheckSpherevsAABBMTV(charColl.worldSphere_, obstColl.worldAabb_, mtv);
                        if (solved) mtv = -mtv; // 第2引数(障害物)を押し出す方向で返るため反転
                    }
                } 
                else if (charColl.type_ == ColliderType::OBB) {
                    if (obstColl.type_ == ColliderType::OBB) {
                        if (useSubstep) solved = collisionAlgorithm::CheckOBBvsOBBSubstepMTV(obstColl.worldObb_, obstColl.previousPosition_, charColl.worldObb_, charColl.previousPosition_, mtv);
                        else solved = collisionAlgorithm::CheckOBBvsOBBMTV(obstColl.worldObb_, charColl.worldObb_, mtv);
                    }
                    else if (obstColl.type_ == ColliderType::AABB) {
                        if (useSubstep) solved = collisionAlgorithm::CheckAABBvsOBBSubstepMTV(obstColl.worldAabb_, obstColl.previousPosition_, charColl.worldObb_, charColl.previousPosition_, mtv);
                        else solved = collisionAlgorithm::CheckAABBvsOBBMTV(obstColl.worldAabb_, charColl.worldObb_, mtv);
                    }
                }
                else if (charColl.type_ == ColliderType::AABB) {
                    if (obstColl.type_ == ColliderType::AABB) {
                        if (useSubstep) solved = collisionAlgorithm::CheckAABBvsAABBSubstepMTV(obstColl.worldAabb_, obstColl.previousPosition_, charColl.worldAabb_, charColl.previousPosition_, mtv);
                        else solved = collisionAlgorithm::CheckAABBvsAABBMTV(obstColl.worldAabb_, charColl.worldAabb_, mtv);
                    }
                    else if (obstColl.type_ == ColliderType::OBB) {
                        Vector3 mtvTemp;
                        if (useSubstep) solved = collisionAlgorithm::CheckAABBvsOBBSubstepMTV(charColl.worldAabb_, charColl.previousPosition_, obstColl.worldObb_, obstColl.previousPosition_, mtvTemp);
                        else solved = collisionAlgorithm::CheckAABBvsOBBMTV(charColl.worldAabb_, obstColl.worldObb_, mtvTemp);
                        if (solved) mtv = -mtvTemp; // 反転してキャラクター側にする
                    }
                }

                if (solved) {
                    isColliding = true;
                    // Y 成分の押し出しフィルタリングを削除（不自然な吹っ飛びやすり抜けの原因になるため）

                    auto& charTrans = registry.GetComponent<TransformComponent>(charEnt);
                    
                    // --- 吹っ飛び防止の修正 ---
                    // charTrans.worldMatrix_ は HierarchySystem 時点の古い（stale な）情報の可能性があるため、
                    // そのフレームで計算済みの最新のワールド中心点 (worldSphere.center / worldObb.center 等) に MTV を適用する
                    Vector3 currentWorldPos;
                    if (charColl.type_ == ColliderType::Sphere) currentWorldPos = charColl.worldSphere_.center;
                    else if (charColl.type_ == ColliderType::OBB) currentWorldPos = charColl.worldObb_.center;
                    else currentWorldPos = charColl.worldAabb_.GetCenter();

                    Vector3 newWorldPos = currentWorldPos + mtv;

                    // worldMatrix_ の並進成分を最新に更新
                    charTrans.worldMatrix_.m[3][0] = newWorldPos.x;
                    charTrans.worldMatrix_.m[3][1] = newWorldPos.y;
                    charTrans.worldMatrix_.m[3][2] = newWorldPos.z;

                    // Parent-Relative な座標の再計算 (逆行列キャッシュ利用)
                    bool hasParent = false;
                    Matrix4x4 parentInvMat = MakeIdentity4x4();
                    if (registry.HasComponent<HierarchyComponent>(charEnt)) {
                        EntityID parentId = registry.GetComponent<HierarchyComponent>(charEnt).parent_;
                        if (parentId != kInvalidEntity && registry.HasComponent<TransformComponent>(parentId)) {
                            if (parentId == lastInvEntity) {
                                parentInvMat = cachedInvMat;
                            } else {
                                parentInvMat = Inverse(registry.GetComponent<TransformComponent>(parentId).worldMatrix_);
                                lastInvEntity = parentId;
                                cachedInvMat = parentInvMat;
                            }
                            hasParent = true;
                        }
                    }

                    if (hasParent) {
                        // 親がいる場合は、最新のワールド位置を親の逆行列でローカル空間に戻す
                        charTrans.localPosition_ = MathUtils::Transform(newWorldPos, parentInvMat);
                    } else {
                        charTrans.localPosition_ = newWorldPos;
                    }
                    charTrans.isDirty_ = true;

                    // 次の判定ペアのためにコライダー形状を更新
                    if (charColl.type_ == ColliderType::AABB) { charColl.worldAabb_.min_ += mtv; charColl.worldAabb_.max_ += mtv; }
                    else if (charColl.type_ == ColliderType::Sphere) charColl.worldSphere_.center += mtv;
                    else if (charColl.type_ == ColliderType::OBB) charColl.worldObb_.center += mtv;

                    // 接地判定
                    if (registry.HasComponent<MovementComponent>(charEnt)) {
                        if (mtv.y > 0.05f) registry.GetComponent<MovementComponent>(charEnt).isGrounded_ = true;
                    }
                }
            } else {
                // --- トリガーまたは移動物同士 (Boolean判定のみ) ---
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
                    if (useSubstep) isColliding = collisionAlgorithm::CheckOBBvsOBBSubstep(colliderA.worldObb_, colliderA.previousPosition_, colliderB.worldObb_, colliderB.previousPosition_);
                    else isColliding = collisionAlgorithm::CheckOBBvsOBB(colliderA.worldObb_, colliderB.worldObb_);
                }
                else if (colliderA.type_ == ColliderType::AABB && colliderB.type_ == ColliderType::OBB) {
                    if (useSubstep) isColliding = collisionAlgorithm::CheckAABBvsOBBSubstep(colliderA.worldAabb_, colliderA.previousPosition_, colliderB.worldObb_, colliderB.previousPosition_);
                    else isColliding = collisionAlgorithm::CheckAABBvsOBB(colliderA.worldAabb_, colliderB.worldObb_);
                }
                else if (colliderA.type_ == ColliderType::OBB && colliderB.type_ == ColliderType::AABB) {
                    if (useSubstep) isColliding = collisionAlgorithm::CheckAABBvsOBBSubstep(colliderB.worldAabb_, colliderB.previousPosition_, colliderA.worldObb_, colliderA.previousPosition_);
                    else isColliding = collisionAlgorithm::CheckAABBvsOBB(colliderB.worldAabb_, colliderA.worldObb_);
                }
                else if (colliderA.type_ == ColliderType::Sphere && colliderB.type_ == ColliderType::OBB) {
                    if (useSubstep) isColliding = collisionAlgorithm::CheckSpherevsOBBSubstep(colliderA.worldSphere_, colliderA.previousPosition_, colliderB.worldObb_, colliderB.previousPosition_);
                    else isColliding = collisionAlgorithm::CheckSpherevsOBB(colliderA.worldSphere_, colliderB.worldObb_);
                }
                else if (colliderA.type_ == ColliderType::OBB && colliderB.type_ == ColliderType::Sphere) {
                    if (useSubstep) isColliding = collisionAlgorithm::CheckSpherevsOBBSubstep(colliderB.worldSphere_, colliderB.previousPosition_, colliderA.worldObb_, colliderA.previousPosition_);
                    else isColliding = collisionAlgorithm::CheckSpherevsOBB(colliderB.worldSphere_, colliderA.worldObb_);
                }
            }

            if (isColliding) {
                if (registry.HasComponent<CollisionResponseComponent>(entityA))
                    registry.GetComponent<CollisionResponseComponent>(entityA).currentCollisions_.insert(entityB);
                if (registry.HasComponent<CollisionResponseComponent>(entityB))
                    registry.GetComponent<CollisionResponseComponent>(entityB).currentCollisions_.insert(entityA);
            }
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
    if (!registry.HasComponentArray<ColliderComponent>()) return;

    auto& colliderArray = registry.GetArray<ColliderComponent>();
    auto* lineManager = LineManager::GetInstance();
    
    for (uint32_t i = 0; i < colliderArray.GetSize(); ++i) {
        EntityID entity = colliderArray.GetEntityFromDenseIndex(i);
        auto& collider = colliderArray.GetDataFromDenseIndex(i);
        if (!collider.isActive_ || !registry.HasComponent<TransformComponent>(entity)) continue;

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
