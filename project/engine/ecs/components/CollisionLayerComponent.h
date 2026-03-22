#pragma once

#include <cstdint>

/**
 * @brief 衝突判定のフィルタリング（レイヤー）を管理するコンポーネント。
 */
struct CollisionLayerComponent
{
    // 自身のカテゴリ（ビットフラグ）
    static constexpr uint32_t kNone = 0;
    static constexpr uint32_t kPlayer = 1 << 0;
    static constexpr uint32_t kEnemy = 1 << 1;
    static constexpr uint32_t kObstacle = 1 << 2;
    static constexpr uint32_t kPlayerBullet = 1 << 3;
    static constexpr uint32_t kEnemyBullet = 1 << 4;
    
    uint32_t category_ = kNone;
    // 衝突を検知するターゲットのマスク
    uint32_t mask_ = 0xFFFFFFFF;
};
