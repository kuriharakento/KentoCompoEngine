#pragma once

#include <cstdint>

// No namespaces

/**
 * @brief エンティティの種別を表すタグコンポーネント。
 *
 * 型ではなく数値タグで種別を持つことで、Systemが特定種別だけを
 * O(1) でフィルタリングできる。
 */
struct TagComponent
{
    // エンティティ種別
    enum class Type : uint32_t
    {
        Unknown  = 0,
        Player   = 1,
        Enemy    = 2,
        Obstacle = 3,
        Bullet   = 4,
        Effect   = 5,
    };

    Type type = Type::Unknown;
};
