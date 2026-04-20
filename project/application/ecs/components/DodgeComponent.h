#pragma once
#include "math/Vector3.h"

/**
 * @brief 回避（Dodge）アクションの状態とパラメータを管理するコンポーネント。
 */
struct DodgeComponent
{
    // 回避フラグ・タイマー
    bool isDodging_ = false;
    float timer_ = 0.0f; // 現在の回避残り時間
    float cooldownTimer_ = 0.0f; // 次回回避までのクールタイム

    // 回避パラメータ
    Vector3 direction_ = { 0, 0, 0 };
    Vector3 startPosition_ = { 0, 0, 0 };
    Vector3 targetPosition_ = { 0, 0, 0 };

    // 定数
    static constexpr float kDistance = 8.0f;
    static constexpr float kDuration = 0.25f;
    static constexpr float kCooldown = 0.8f;
    static constexpr float kInvincibleTime = 0.25f;
};
