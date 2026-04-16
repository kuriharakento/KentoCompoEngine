#pragma once
#include "math/Vector3.h"

/**
 * @brief 弾丸（Projectile）の性質を管理するコンポーネント。
 */
struct ProjectileComponent
{
    enum class Type { Lmb, Rmb, Decoy, Impact, Beam };
    
    // 基本物理量
    Vector3 velocity_ = { 0, 0, 0 };
    float speed_ = 50.0f;
    float damage_ = 10.0f;
    float currentRadius_ = 0.5f;

    // 寿命管理
    float lifetime_ = 2.0f;
    float currentLifetime_ = 0.0f;
    bool isAlive_ = true;

    // タイプ
    Type type_ = Type::Lmb;

    // 演出用
    enum class TrailType { Bullet, Homing, None };
    TrailType trailType_ = TrailType::Bullet;
    uint32_t trailId_ = 0;

    // 貫通性能
    uint32_t pierceCount_ = 0;

    // ホーミング用
    EntityID targetEntity_ = kInvalidEntity;
    bool isHoming_ = false;
};
