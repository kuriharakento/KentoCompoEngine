#pragma once
#include "Vector3.h"

/// 球のデフォルト半径
constexpr float kDefaultRadius = 1.0f;

/**
 * @brief 球体バウンディングボリューム
 * 
 * 3D空間における球体を表現する構造体。
 * 中心座標と半径で定義され、最も単純な衝突判定に使用される。
 * 回転に対して不変であり、高速な衝突判定が可能。
 */
struct Sphere
{
    Vector3 center; // 中心座標
    float radius;   // 半径

    /**
     * @brief デフォルトコンストラクタ（原点、半径1.0の球を生成）
     */
    Sphere() : center(0, 0, 0), radius(kDefaultRadius) {}

    /**
     * @brief 中心と半径を指定するコンストラクタ
     * @param c 中心座標
     * @param r 半径
     */
    Sphere(const Vector3& c, float r) : center(c), radius(r) {}
};
