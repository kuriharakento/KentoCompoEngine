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
    // ローカル座標
    Vector3 localPosition_ = { 0.0f, 0.0f, 0.0f };
    // ローカル回転角（ラジアン）
    Vector3 localRotation_ = { 0.0f, 0.0f, 0.0f };
    // ローカルスケール
    Vector3 localScale_ = { 1.0f, 1.0f, 1.0f };

    // キャッシュされた行列
    Matrix4x4 localMatrix_ = MakeIdentity4x4();
    Matrix4x4 worldMatrix_ = MakeIdentity4x4();

    // [BNS-Optimization] 変更があったかどうかのフラグ
    // これが false の間、HierarchySystem は行列の再計算をスキップできる。
    bool isDirty_ = true;
};
