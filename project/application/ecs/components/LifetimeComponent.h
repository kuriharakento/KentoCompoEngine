#pragma once

// No namespaces

/**
 * @brief エンティティの寿命を管理するコンポーネント。
 * 
 * 毎フレーム currentAge に加算され、maxLifetime を超えたら遅延破棄される。
 * 用途: 弾、エフェクト、ダメージUIなど。
 */
struct LifetimeComponent
{
    // 生成されてからの経過時間（秒）
    float currentAge = 0.0f;

    // 寿命（秒）。これを超えると破棄予約される
    float maxLifetime = 10.0f;
};


