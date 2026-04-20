#pragma once
#include "application/gameObject/combatable/base/StatusSystem.h"

namespace ecs
{
/**
 * @brief エンティティのステータス（HP、移動速度など）を管理するコンポーネント。
 */
struct StatusComponent
{
    StatusComponent() = default;
    ~StatusComponent() = default;

    // ムーブを許可
    StatusComponent(StatusComponent&&) noexcept = default;
    StatusComponent& operator=(StatusComponent&&) noexcept = default;

    // コピーは禁止（StatusValueにunique_ptrが含まれるため）
    StatusComponent(const StatusComponent&) = delete;
    StatusComponent& operator=(const StatusComponent&) = delete;
    // 現在のHP
    StatusValue hp_{ 100.0f };
    // 最大HP
    StatusValue maxHp_{ 100.0f };
    // 攻撃力
    StatusValue attackPower_{ 10.0f };
    // 移動速度
    StatusValue moveSpeed_{ 5.0f };
    
    // 生存フラグ
    bool isAlive_ = true;
};
}
