#pragma once
#include <cstdint>

/**
 * @brief 敵に付与されたインパクト・チャージ（爆弾）の状態を管理。
 */
struct ImpactChargeComponent
{
    // 現在の蓄積スタック数 (0~4)
    uint8_t stackCount_ = 0;
    static constexpr uint8_t kMaxStacks = 4;

    // 爆発属性
    float explosionRadius_ = 5.0f;
    float explosionDamage_Base_ = 50.0f;

    // 付与済みフラグ
    bool isPrimed_ = false;
};
