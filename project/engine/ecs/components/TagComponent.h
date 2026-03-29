#pragma once

#include <cstdint>

namespace ecs
{
/**
 * @brief エンティティの種別を表すタグコンポーネント。
 *
 * 名称が既存エンジンと衝突しやすいため、EcsTagComponent として定義。
 */
struct EcsTagComponent
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
}
