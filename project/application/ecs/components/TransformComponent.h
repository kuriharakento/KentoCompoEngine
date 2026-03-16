#pragma once

#include "math/MatrixFunc.h"
#include "math/Vector3.h"
#include "math/Quaternion.h"

/**
 * @brief 位置・姿勢・スケール情報を持つ汎用コンポーネント。
 * 
 * PODとして設計し、メモリの連続配置と高速コピーを保証する。
 */
struct TransformComponent
{
    // ローカル空間情報
    Vector3 localPosition = { 0.0f, 0.0f, 0.0f };
    Vector3 localRotation = { 0.0f, 0.0f, 0.0f }; // Euler angles (radians)
    Vector3 localScale    = { 1.0f, 1.0f, 1.0f };

    // 計算済み行列キャッシュ（毎フレーム再計算される）
    Matrix4x4 localMatrix = MakeIdentity4x4();
    Matrix4x4 worldMatrix = MakeIdentity4x4();
};
