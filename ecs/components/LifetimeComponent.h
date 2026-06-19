#pragma once

namespace ecs
{
    /**
     * @brief エンティティの寿命を管理するコンポーネント。
     * 
     * 毎フレーム currentAge に加算され、maxLifetime を超えたら遅延破棄される。
     * 用途: 弾、エフェクト、ダメージUIなど。
     */
    struct LifetimeComponent
    {
        // 現在の生存時間
        float currentAge_ = 0.0f;

        // 最大寿命
        float maxLifetime_ = 10.0f;
    };
}
