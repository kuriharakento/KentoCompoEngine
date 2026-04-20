#pragma once

#include "math/Vector3.h"

/**
 * @brief 突進型の敵（Charger）専用の状態管理コンポーネント。
 */
struct EnemyChargerComponent
{
    // 現在のステート（0: 照準・待機, 1: 突進中, 2: 突進後の硬直）
    int state_ = 0;

    // 状態遷移の時間を計るタイマー
    float timer_ = 0.0f;

    // 突進する方向のベクトル
    Vector3 chargeDirection_ = { 0.0f, 0.0f, 0.0f };
};
