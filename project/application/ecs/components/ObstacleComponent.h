#pragma once

#include <cstdint>

// No namespaces

/**
 * @brief 障害物固有の属性を保持するコンポーネント。
 *
 * 種別（Obstacle / BarrierBlock / Floor）とコライダー有無を管理する。
 * Phoneは単なるデータの入れ物で、ロジックを持たない。
 */
struct ObstacleComponent
{
    // 障害物の種別
    enum class Type : uint32_t
    {
        Obstacle     = 0, // 通常障害物（コライダーあり）
        BarrierBlock = 1, // バリアブロック（コライダーあり）
        Floor        = 2, // 床（コライダーなし）
    };

    Type type = Type::Obstacle;

    // true なら CollisionManager に登録済み
    bool hasCollider = true;
};
