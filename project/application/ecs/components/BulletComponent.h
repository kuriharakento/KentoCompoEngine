#pragma once

#include <cstdint>
#include "engine/ecs/Entity.h"

/**
 * @brief 弾丸の属性を管理するECSコンポーネント。
 */
struct BulletComponent
{
    // ダメージ量
    float damage_ = 10.0f;
    
    // 生存フラグ（CollisionResponseSystemなどで衝突時にfalseに設定される）
    bool isAlive_ = true;
    
    // 発射元のエンティティID（自分自身への誤射防止などに使用）
    EntityID owner_ = kInvalidEntity;
    
    // 弾丸の種別（Player用/Enemy用など、タグに近い運用も可能）
    enum class Type {
        Player,
        Enemy
    } type_ = Type::Player;
};
