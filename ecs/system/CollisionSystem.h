#pragma once

#include "ISystem.h"
#include "SpatialPartition.h"
#include "engine/ecs/Entity.h"
#include "math/Vector3.h"

#include <mutex>
#include <set>
#include <functional>

/**
 * @brief 空間分割（Grid）を用いて効率的に衝突判定を行うシステム。
 */
class CollisionSystem : public ISystem
{
public:
    struct CollisionEvent {
        EntityID a, b;
        bool isSolid;
        Vector3 mtv;
    };

    CollisionSystem();
    void UpdatePreviousPositions(Registry& registry);
    void Update(Registry& registry) override;
    void Draw(Registry& registry, Camera* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager) override;

    /**
     * @brief 指定した球体範囲内にあるエンティティを検索する。
     * @param position 中心座標
     * @param radius 半径
     * @param callback 見つかったエンティティごとに呼ばれるコールバック
     */
    void QueryNearbyEntities(const Vector3& position, float radius, const std::function<void(EntityID)>& callback) const;

    // スレッドごとに独立して保持する判定用ワークエリア
    struct ThreadLocalContext {
        std::vector<EntityID> neighbors;
    };

private:
    // 判定フェーズ (Parallel)
    void DetectCollisions(Registry& registry);
    // レスポンスフェーズ (Serial)
    void ResolveCollisions(Registry& registry);

    // 判定結果
    std::vector<CollisionEvent> collisions_;
    std::mutex mergeMutex_;
    
    // 前フレーム位置更新用の一時インデックス
    std::vector<uint32_t> colliderIndices_;

    std::unique_ptr<LinearSpatialHash> grid_;
};
