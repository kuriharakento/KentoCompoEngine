#pragma once

#include "math/Vector3.h"

namespace KCE
{
namespace ecs
{
    /**
     * @brief 物理移動（速度・加速度）を制御するコンポーネント。
     */
    struct MovementComponent
    {
        // 現在の速度
        Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
        // 加速度（毎フレームリセットされる想定）
        Vector3 acceleration_ = { 0.0f, 0.0f, 0.0f };
        // 最大移動速度
        float maxSpeed_ = 100.0f;
        // 摩擦・抵抗係数 (0.0f ~ 1.0f)
        float friction_ = 0.95f;
        
        // 地面に接しているかなどのフラグ
        bool isGrounded_ = false;
        // 重力を適用するか
        bool useGravity_ = false;
        // 重力定数 (デフォルト 9.8)
        float gravity_ = 9.8f;
    };
}
} // namespace KCE
