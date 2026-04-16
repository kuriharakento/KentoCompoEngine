#pragma once

namespace ecs
{
/**
 * @brief 敵に付与される誘爆スタックを管理するコンポーネント。
 */
struct InducedExplosionComponent
{
    // 現在のスタック数
    int count_ = 0;
    
    // 最大スタック数
    static constexpr int kMaxCount = 3;
    
    // 誘爆ダメージ
    static constexpr float kExplosionDamage = 500.0f;
    
    // 誘爆半径
    static constexpr float kExplosionRadius = 12.0f;
};
}
