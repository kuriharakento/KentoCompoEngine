#pragma once

#include <cstdint>

namespace ecs
{
/**
 * @brief エンティティの種別を表すタグコンポーネント。
 *
 * 名称が既存エンジンと衝突しやすいため、ecs 名前空間内で定義。
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
        Decoy    = 6,
    };

    Type type = Type::Unknown;
};
}
