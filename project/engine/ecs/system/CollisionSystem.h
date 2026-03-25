#pragma once

#include "ISystem.h"
#include "SpatialPartition.h"

/**
 * @brief 空間分割（Grid）を用いて効率的に衝突判定を行うシステム。
 */
class CollisionSystem : public ISystem
{
public:
    CollisionSystem();
    void UpdatePreviousPositions(Registry& registry);
    void Update(Registry& registry) override;
    void Draw(Registry& registry, Camera* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager) override;

private:
    std::unique_ptr<LinearSpatialHash> grid_;

    // --- パフォーマンス最適化用の再利用バッファ ---
    // 毎フレームのアロケーションを避けるため、メンバ変数として保持する
    mutable std::vector<EntityID> neighbors_;
    mutable std::vector<uint32_t> marker_;
    uint32_t markerFrame_ = 0;
};
